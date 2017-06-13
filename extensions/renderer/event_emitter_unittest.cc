// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_emitter.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_event_listeners.h"
#include "gin/handle.h"

namespace extensions {
namespace {

void DoNothingOnListenerChange(binding::EventListenersChanged changed,
                               const base::DictionaryValue* filter,
                               bool was_manual,
                               v8::Local<v8::Context> context) {}

}  // namespace

using EventEmitterUnittest = APIBindingTest;

TEST_F(EventEmitterUnittest, TestDispatchMethod) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto listeners = base::MakeUnique<UnfilteredEventListeners>(
      base::Bind(&DoNothingOnListenerChange), binding::kNoListenerMax);

  // The test util methods enforce that functions always throw or always don't
  // throw, but we test listeners that do both. Provide implementations for
  // running functions that don't enforce throw behavior.
  auto run_js_sync = [](v8::Local<v8::Function> function,
                        v8::Local<v8::Context> context, int argc,
                        v8::Local<v8::Value> argv[]) {
    v8::Global<v8::Value> global_result;
    v8::Local<v8::Value> result;
    if (function->Call(context, context->Global(), argc, argv).ToLocal(&result))
      global_result.Reset(context->GetIsolate(), result);
    return global_result;
  };

  auto run_js = [](v8::Local<v8::Function> function,
                   v8::Local<v8::Context> context, int argc,
                   v8::Local<v8::Value> argv[]) {
    ignore_result(function->Call(context, context->Global(), argc, argv));
  };

  gin::Handle<EventEmitter> event = gin::CreateHandle(
      isolate(), new EventEmitter(false, std::move(listeners),
                                  base::Bind(run_js), base::Bind(run_js_sync)));

  v8::Local<v8::Value> v8_event = event.ToV8();

  const char kAddListener[] =
      "(function(event, listener) { event.addListener(listener); })";
  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListener);

  auto add_listener = [context, v8_event,
                       add_listener_function](base::StringPiece listener) {
    v8::Local<v8::Function> listener_function =
        FunctionFromString(context, listener);
    v8::Local<v8::Value> args[] = {v8_event, listener_function};
    RunFunction(add_listener_function, context, arraysize(args), args);
  };

  const char kListener1[] =
      "(function() {\n"
      "  this.eventArgs1 = Array.from(arguments);\n"
      "  return 'listener1';\n"
      "})";
  add_listener(kListener1);
  const char kListener2[] =
      "(function() {\n"
      "  this.eventArgs2 = Array.from(arguments);\n"
      "  return {listener: 'listener2'};\n"
      "})";
  add_listener(kListener2);
  // Listener3 throws, but shouldn't stop the event from reaching other
  // listeners.
  const char kListener3[] =
      "(function() {\n"
      "  this.eventArgs3 = Array.from(arguments);\n"
      "  throw new Error('hahaha');\n"
      "})";
  add_listener(kListener3);
  // Returning undefined should not be added to the array of results from
  // dispatch.
  const char kListener4[] =
      "(function() {\n"
      "  this.eventArgs4 = Array.from(arguments);\n"
      "})";
  add_listener(kListener4);

  const char kDispatch[] =
      "(function(event) {\n"
      "  return event.dispatch('arg1', 2);\n"
      "})";
  v8::Local<v8::Value> dispatch_args[] = {v8_event};
  v8::Local<v8::Value> dispatch_result =
      RunFunctionOnGlobal(FunctionFromString(context, kDispatch), context,
                          arraysize(dispatch_args), dispatch_args);

  const char kExpectedEventArgs[] = "[\"arg1\",2]";
  for (const char* property :
       {"eventArgs1", "eventArgs2", "eventArgs3", "eventArgs4"}) {
    EXPECT_EQ(kExpectedEventArgs, GetStringPropertyFromObject(
                                      context->Global(), context, property));
  }
  EXPECT_EQ("{\"results\":[\"listener1\",{\"listener\":\"listener2\"}]}",
            V8ToString(dispatch_result, context));
}

}  // namespace extensions
