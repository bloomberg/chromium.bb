// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/safe_builtins.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"

namespace extensions {

using namespace v8_helpers;

namespace {

// Preface:
//
// This is the documentation for makeCallback() function in the JavaScript, out
// here to reduce the amount of effort that the v8 parser needs to do:
//
// Returns a new object with every function on |obj| configured to call()
// itself with the given arguments.
//
// E.g. given
//    var result = makeCallable(Function.prototype)
//
// |result| will be an object including 'bind' such that
//    result.bind(foo, 1, 2, 3);
//
// is equivalent to Function.prototype.bind.call(foo, 1, 2, 3), and so on.
// This is a convenient way to save functions that user scripts may clobber.

// This is the source of a script which evaluates to a function, which should
// then be executed to save the bindings. This function is passed references
// to ScriptRunner::Apply and ScriptRunner::Save via the |natives| argument.
const char kScript[] =
    "(function(natives) {\n"
    "'use strict';\n"
    "\n"
    "// Used in the callback implementation, could potentially be clobbered.\n"
    "function makeCallable(obj, target, isStatic, propertyNames) {\n"
    "  propertyNames.forEach(function(propertyName) {\n"
    "    var property = obj[propertyName];\n"
    "    target[propertyName] = function() {\n"
    "      var recv = obj;\n"
    "      var firstArgIndex = 0;\n"
    "      if (!isStatic) {\n"
    "        if (arguments.length == 0)\n"
    "          throw 'There must be at least one argument, the receiver';\n"
    "        recv = arguments[0];\n"
    "        firstArgIndex = 1;\n"
    "      }\n"
    "      return natives.Apply(\n"
    "          property, recv, arguments, firstArgIndex, arguments.length);\n"
    "    };\n"
    "  });\n"
    "}\n"
    "\n"
    "function saveBuiltin(builtin, protoPropertyNames, staticPropertyNames) {\n"
    "  var safe = function() {\n"
    "    throw 'Safe objects cannot be called nor constructed. ' +\n"
    "          'Use $Foo.self() or new $Foo.self() instead.';\n"
    "  };\n"
    "  safe.self = builtin;\n"
    "  makeCallable(builtin.prototype, safe, false, protoPropertyNames);\n"
    "  if (staticPropertyNames)\n"
    "    makeCallable(builtin, safe, true, staticPropertyNames);\n"
    "  natives.Save(builtin.name, safe);\n"
    "}\n"
    "\n"
    "// Save only what is needed by the extension modules.\n"
    "saveBuiltin(Object,\n"
    "            ['hasOwnProperty'],\n"
    "            ['create', 'defineProperty', 'freeze',\n"
    "             'getOwnPropertyDescriptor', 'getPrototypeOf', 'keys']);\n"
    "saveBuiltin(Function,\n"
    "            ['apply', 'bind', 'call']);\n"
    "saveBuiltin(Array,\n"
    "            ['concat', 'forEach', 'indexOf', 'join', 'push', 'slice',\n"
    "             'splice', 'map', 'filter']);\n"
    "saveBuiltin(String,\n"
    "            ['indexOf', 'slice', 'split', 'substr', 'toUpperCase',\n"
    "             'replace']);\n"
    "saveBuiltin(RegExp,\n"
    "            ['test']);\n"
    "saveBuiltin(Error,\n"
    "            [],\n"
    "            ['captureStackTrace']);\n"
    "\n"
    "// JSON is trickier because extensions can override toJSON in\n"
    "// incompatible ways, and we need to prevent that.\n"
    "var builtinTypes = [\n"
    "  Object, Function, Array, String, Boolean, Number, Date, RegExp\n"
    "];\n"
    "var builtinToJSONs = builtinTypes.map(function(t) {\n"
    "  return t.toJSON;\n"
    "});\n"
    "var builtinArray = Array;\n"
    "var builtinJSONStringify = JSON.stringify;\n"
    "natives.Save('JSON', {\n"
    "  parse: JSON.parse,\n"
    "  stringify: function(obj) {\n"
    "    var savedToJSONs = new builtinArray(builtinTypes.length);\n"
    "    try {\n"
    "      for (var i = 0; i < builtinTypes.length; ++i) {\n"
    "        try {\n"
    "          if (builtinTypes[i].prototype.toJSON !==\n"
    "              builtinToJSONs[i]) {\n"
    "            savedToJSONs[i] = builtinTypes[i].prototype.toJSON;\n"
    "            builtinTypes[i].prototype.toJSON = builtinToJSONs[i];\n"
    "          }\n"
    "        } catch (e) {}\n"
    "      }\n"
    "    } catch (e) {}\n"
    "    try {\n"
    "      return builtinJSONStringify(obj);\n"
    "    } finally {\n"
    "      for (var i = 0; i < builtinTypes.length; ++i) {\n"
    "        try {\n"
    "          if (i in savedToJSONs)\n"
    "            builtinTypes[i].prototype.toJSON = savedToJSONs[i];\n"
    "        } catch (e) {}\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "});\n"
    "\n"
    "});\n";

// Holds a compiled instance of |kScript| so that every instance of
// SafeBuiltins doesn't need to recompile it. Thread-safe.
// TODO(kalman): It would benefit to cache ModuleSystem's native handlers in
// this way as well.
class CompiledScript {
 public:
  CompiledScript() {}

  // Returns a handle to the instance of the compiled script, bound to the
  // current context (assumed to be |context|).
  v8::Local<v8::Script> GetForCurrentContext(v8::Local<v8::Context> context) {
    v8::Isolate* isolate = context->GetIsolate();
    DCHECK(v8::Isolate::GetCurrent() == isolate &&
           isolate->GetCurrentContext() == context);
    v8::EscapableHandleScope handle_scope(isolate);

    v8::Local<v8::Script> compiled_script;
    base::AutoLock lock_scope(lock_);
    if (unbound_compiled_script_.IsEmpty()) {
      compiled_script =
          v8::Script::Compile(context, ToV8StringUnsafe(isolate, kScript))
              .ToLocalChecked();
      unbound_compiled_script_.Reset(isolate,
                                     compiled_script->GetUnboundScript());
    } else {
      compiled_script =
          v8::Local<v8::UnboundScript>::New(isolate, unbound_compiled_script_)
              ->BindToCurrentContext();
    }
    return handle_scope.Escape(compiled_script);
  }

 private:
  // CompiledScript needs to be accessed on multiple threads - the main
  // RenderThread, plus worker threads. Singletons are thread-safe, but
  // access to |unbound_compiled_script_| must be locked.
  base::Lock lock_;

  // Use a v8::Persistent not a v8::Global because Globals attempt to reset the
  // handle on destruction, and by the time CompiledScript is destroyed the
  // renderer will be shutting down, and accessing into v8 will crash.
  v8::Persistent<v8::UnboundScript> unbound_compiled_script_;

  DISALLOW_COPY_AND_ASSIGN(CompiledScript);
};

base::LazyInstance<CompiledScript> g_compiled_script =
    LAZY_INSTANCE_INITIALIZER;

// Returns a unique key to use as a hidden value in an object without a
// namespace collision.
v8::Local<v8::String> MakeKey(const char* name, v8::Isolate* isolate) {
  return ToV8StringUnsafe(isolate,
                          base::StringPrintf("safe_builtins::%s", name));
}

class ScriptNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit ScriptNativeHandler(ScriptContext* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction("Apply", base::Bind(&ScriptNativeHandler::Apply,
                                      base::Unretained(this)));
    RouteFunction(
        "Save", base::Bind(&ScriptNativeHandler::Save, base::Unretained(this)));
  }

  ~ScriptNativeHandler() override { Invalidate(); }

 private:
  // Takes 5 arguments:
  //  |function| The function that the arguments are being applied to.
  //  |recv| The receiver of the function call (i.e. the "this" value).
  //  |args| The arguments to the function call. This is actually an Arguments
  //    object but that isn't exposed in a convenient way in the v8 API, so we
  //    just use an Object and pass in |args_length| explicitly.
  //  |first_arg_index| The index of the first argument within |args|.
  //    This is 1 for prototype methods where the first argument to the
  //    function is the receiver. It's 0 for static methods which don't have a
  //    receiver.
  //  |args_length| The length of the argument list. This is needed because
  //    |args| is an Object which doesn't have a reliable concept of a length.
  void Apply(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(info.Length() == 5 && info[0]->IsFunction() && info[2]->IsObject() &&
          info[3]->IsInt32() && info[4]->IsInt32());
    v8::Local<v8::Function> function = info[0].As<v8::Function>();
    v8::Local<v8::Value> recv = info[1];
    v8::Local<v8::Object> args = info[2].As<v8::Object>();
    int first_arg_index = info[3].As<v8::Int32>()->Value();
    int args_length = info[4].As<v8::Int32>()->Value();

    v8::Local<v8::Context> v8_context = context()->v8_context();

    int argc = args_length - first_arg_index;
    scoped_ptr<v8::Local<v8::Value> []> argv(new v8::Local<v8::Value>[argc]);
    for (int i = 0; i < argc; ++i) {
      CHECK(IsTrue(args->Has(v8_context, i + first_arg_index)));
      // Getting a property value could throw an exception.
      if (!GetProperty(v8_context, args, i + first_arg_index, &argv[i]))
        return;
    }

    v8::Local<v8::Value> return_value;
    if (function->Call(v8_context, recv, argc, argv.get())
            .ToLocal(&return_value))
      info.GetReturnValue().Set(return_value);
  }

  void Save(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(info.Length() == 2 && info[0]->IsString() && info[1]->IsObject());
    v8::Local<v8::Object> object = info[1].As<v8::Object>();
    context()->v8_context()->Global()->SetHiddenValue(
        MakeKey(*v8::String::Utf8Value(info[0]), GetIsolate()), object);
  }

  DISALLOW_COPY_AND_ASSIGN(ScriptNativeHandler);
};

void DeleteScriptHandler(scoped_ptr<ScriptNativeHandler> script_handler) {
  // |script_handler| is a scoped_ptr so will delete itself.
}

}  // namespace

SafeBuiltins::~SafeBuiltins() {}

// static
scoped_ptr<SafeBuiltins> SafeBuiltins::Install(ScriptContext* context) {
  v8::Isolate* isolate = context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> v8_context = context->v8_context();
  v8::Context::Scope context_scope(v8_context);
  blink::WebScopedMicrotaskSuppression microtask_suppression;

  // Run the script to return a new function bound to this context.
  v8::Local<v8::Script> script =
      g_compiled_script.Get().GetForCurrentContext(v8_context);
  v8::Local<v8::Value> return_value = script->Run();
  CHECK(return_value->IsFunction());
  v8::Local<v8::Function> script_function = return_value.As<v8::Function>();

  // Call the script function to save builtins.
  scoped_ptr<ScriptNativeHandler> script_handler(
      new ScriptNativeHandler(context));
  v8::Local<v8::Value> args[] = {script_handler->NewInstance()};
  CHECK(!script_function->Call(v8_context, v8_context->Global(),
                               arraysize(args), args)
             .IsEmpty());

  // Bind the lifetime of |script_handler| to |context|.
  context->AddInvalidationObserver(
      base::Bind(&DeleteScriptHandler, base::Passed(&script_handler)));

  // The SafeBuiltins instance itself is just a thin wrapper around accessing
  // the hidden properties that were just installed on |context|.
  return make_scoped_ptr(new SafeBuiltins(context));
}

v8::Local<v8::Object> SafeBuiltins::GetArray() const {
  return Load("Array");
}

v8::Local<v8::Object> SafeBuiltins::GetFunction() const {
  return Load("Function");
}

v8::Local<v8::Object> SafeBuiltins::GetJSON() const {
  return Load("JSON");
}

v8::Local<v8::Object> SafeBuiltins::GetObjekt() const {
  return Load("Object");
}

v8::Local<v8::Object> SafeBuiltins::GetRegExp() const {
  return Load("RegExp");
}

v8::Local<v8::Object> SafeBuiltins::GetString() const {
  return Load("String");
}

v8::Local<v8::Object> SafeBuiltins::GetError() const {
  return Load("Error");
}

SafeBuiltins::SafeBuiltins(ScriptContext* context) : context_(context) {}

v8::Local<v8::Object> SafeBuiltins::Load(const char* name) const {
  v8::Local<v8::Value> value = context_->v8_context()->Global()->GetHiddenValue(
      MakeKey(name, context_->isolate()));
  CHECK(!IsEmptyOrUndefined(value));
  CHECK(value->IsObject()) << name;
  return v8::Local<v8::Object>::Cast(value);
}

}  //  namespace extensions
