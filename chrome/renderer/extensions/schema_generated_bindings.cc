// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/schema_generated_bindings.h"

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "grit/common_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

using content::V8ValueConverter;
using extensions::ExtensionAPI;
using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;

namespace {

const char* kExtensionDeps[] = {
  "extensions/event.js",
  "extensions/json_schema.js",
  "extensions/miscellaneous_bindings.js",
  "extensions/apitest.js"
};

// Contains info relevant to a pending API request.
struct PendingRequest {
 public :
  PendingRequest(v8::Persistent<v8::Context> context, const std::string& name,
                 const std::string& extension_id)
      : context(context), name(name), extension_id(extension_id) {
  }
  v8::Persistent<v8::Context> context;
  std::string name;
  std::string extension_id;
};
typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;

base::LazyInstance<PendingRequestMap> g_pending_requests =
    LAZY_INSTANCE_INITIALIZER;

class ExtensionImpl : public ChromeV8Extension {
 public:
  explicit ExtensionImpl(ExtensionDispatcher* extension_dispatcher)
      : ChromeV8Extension("extensions/schema_generated_bindings.js",
                          IDR_SCHEMA_GENERATED_BINDINGS_JS,
                          arraysize(kExtensionDeps),
                          kExtensionDeps,
                          extension_dispatcher) {
  }

  ~ExtensionImpl() {
    // TODO(aa): It seems that v8 never deletes us, so this doesn't get called.
    // Leaving this in here in case v8's implementation ever changes.
    for (CachedSchemaMap::iterator it = schemas_.begin(); it != schemas_.end();
        ++it) {
      if (!it->second.IsEmpty())
        it->second.Dispose();
    }
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("GetExtensionAPIDefinition"))) {
      return v8::FunctionTemplate::New(GetExtensionAPIDefinition,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("GetNextRequestId"))) {
      return v8::FunctionTemplate::New(GetNextRequestId);
    } else if (name->Equals(v8::String::New("StartRequest"))) {
      return v8::FunctionTemplate::New(StartRequest,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("SetIconCommon"))) {
      return v8::FunctionTemplate::New(SetIconCommon,
                                       v8::External::New(this));
    }

    return ChromeV8Extension::GetNativeFunction(name);
  }

 private:
  static v8::Handle<v8::Value> GetV8SchemaForAPI(
      ExtensionImpl* self,
      v8::Handle<v8::Context> context,
      const std::string& api_name) {
    CachedSchemaMap::iterator maybe_api = self->schemas_.find(api_name);
    if (maybe_api != self->schemas_.end())
      return maybe_api->second;

    scoped_ptr<V8ValueConverter> v8_value_converter(V8ValueConverter::create());
    const base::DictionaryValue* schema =
        ExtensionAPI::GetInstance()->GetSchema(api_name);
    CHECK(schema) << api_name;

    self->schemas_[api_name] =
        v8::Persistent<v8::Object>::New(v8::Handle<v8::Object>::Cast(
            v8_value_converter->ToV8Value(schema, context)));
    CHECK(!self->schemas_[api_name].IsEmpty());

    return self->schemas_[api_name];
  }

  static v8::Handle<v8::Value> GetExtensionAPIDefinition(
      const v8::Arguments& args) {
    ExtensionImpl* self = GetFromArguments<ExtensionImpl>(args);
    ExtensionDispatcher* dispatcher = self->extension_dispatcher_;

    ChromeV8Context* v8_context = dispatcher->v8_context_set().GetCurrent();
    CHECK(v8_context);
    std::string extension_id = v8_context->extension_id();
    const ::Extension* extension = NULL;
    if (!extension_id.empty())
      extension = dispatcher->extensions()->GetByID(extension_id);

    ExtensionAPI::SchemaMap schemas;
    if (!extension) {
      LOG(WARNING) << "Extension " << extension_id << " not found";
      ExtensionAPI::GetInstance()->GetDefaultSchemas(&schemas);
    } else {
      ExtensionAPI::GetInstance()->GetSchemasForExtension(*extension, &schemas);
    }

    v8::Persistent<v8::Context> context(v8::Context::New());
    v8::Context::Scope context_scope(context);
    v8::Handle<v8::Array> api(v8::Array::New(schemas.size()));
    size_t api_index = 0;
    for (ExtensionAPI::SchemaMap::iterator it = schemas.begin();
        it != schemas.end(); ++it) {
      api->Set(api_index, GetV8SchemaForAPI(self, context, it->first));
      ++api_index;
    }

    // The persistent extension_api_ will keep the context alive.
    context.Dispose();

    return api;
  }

  static v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args) {
    static int next_request_id = 0;
    return v8::Integer::New(next_request_id++);
  }

  // Common code for starting an API request to the browser. |value_args|
  // contains the request's arguments.
  // Steals value_args contents for efficiency.
  static v8::Handle<v8::Value> StartRequestCommon(
      const v8::Arguments& args, ListValue* value_args) {
    ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);

    const ChromeV8ContextSet& contexts =
        v8_extension->extension_dispatcher()->v8_context_set();
    ChromeV8Context* current_context = contexts.GetCurrent();
    if (!current_context)
      return v8::Undefined();

    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    content::RenderView* renderview = current_context->GetRenderView();
    if (!renderview)
      return v8::Undefined();

    std::string name = *v8::String::AsciiValue(args[0]);
    const std::set<std::string>& function_names =
        v8_extension->extension_dispatcher_->function_names();
    if (function_names.find(name) == function_names.end()) {
      NOTREACHED() << "Unexpected function " << name;
      return v8::Undefined();
    }

    if (!v8_extension->CheckCurrentContextAccessToExtensionAPI(name))
      return v8::Undefined();

    GURL source_url;
    WebSecurityOrigin source_origin;
    WebFrame* webframe = current_context->web_frame();
    if (webframe) {
      source_url = webframe->document().url();
      source_origin = webframe->document().securityOrigin();
    }

    int request_id = args[2]->Int32Value();
    bool has_callback = args[3]->BooleanValue();
    bool for_io_thread = args[4]->BooleanValue();

    v8::Persistent<v8::Context> v8_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!v8_context.IsEmpty());
    g_pending_requests.Get()[request_id].reset(new PendingRequest(
        v8_context, name, current_context->extension_id()));

    ExtensionHostMsg_Request_Params params;
    params.name = name;
    params.arguments.Swap(value_args);
    params.extension_id = current_context->extension_id();
    params.source_url = source_url;
    params.source_origin = source_origin.toString();
    params.request_id = request_id;
    params.has_callback = has_callback;
    params.user_gesture =
        webframe ? webframe->isProcessingUserGesture() : false;
    if (for_io_thread) {
      renderview->Send(new ExtensionHostMsg_RequestForIOThread(
          renderview->GetRoutingId(), params));
    } else {
      renderview->Send(new ExtensionHostMsg_Request(
          renderview->GetRoutingId(), params));
    }

    return v8::Undefined();
  }

  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    std::string str_args = *v8::String::Utf8Value(args[1]);
    base::JSONReader reader;
    scoped_ptr<Value> value_args;
    value_args.reset(reader.JsonToValue(str_args, false, false));

    // Since we do the serialization in the v8 extension, we should always get
    // valid JSON.
    if (!value_args.get() || !value_args->IsType(Value::TYPE_LIST)) {
      NOTREACHED() << "Invalid JSON passed to StartRequest.";
      return v8::Undefined();
    }

    return StartRequestCommon(args, static_cast<ListValue*>(value_args.get()));
  }

  static bool ConvertImageDataToBitmapValue(
      const v8::Arguments& args, Value** bitmap_value) {
    v8::Local<v8::Object> extension_args = args[1]->ToObject();
    v8::Local<v8::Object> details =
        extension_args->Get(v8::String::New("0"))->ToObject();
    v8::Local<v8::Object> image_data =
        details->Get(v8::String::New("imageData"))->ToObject();
    v8::Local<v8::Object> data =
        image_data->Get(v8::String::New("data"))->ToObject();
    int width = image_data->Get(v8::String::New("width"))->Int32Value();
    int height = image_data->Get(v8::String::New("height"))->Int32Value();

    int data_length = data->Get(v8::String::New("length"))->Int32Value();
    if (data_length != 4 * width * height) {
      NOTREACHED() << "Invalid argument to setIcon. Expecting ImageData.";
      return false;
    }

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseARGB(0, 0, 0, 0);

    uint32_t* pixels = bitmap.getAddr32(0, 0);
    for (int t = 0; t < width*height; t++) {
      // |data| is RGBA, pixels is ARGB.
      pixels[t] = SkPreMultiplyColor(
          ((data->Get(v8::Integer::New(4*t + 3))->Int32Value() & 0xFF) << 24) |
          ((data->Get(v8::Integer::New(4*t + 0))->Int32Value() & 0xFF) << 16) |
          ((data->Get(v8::Integer::New(4*t + 1))->Int32Value() & 0xFF) << 8) |
          ((data->Get(v8::Integer::New(4*t + 2))->Int32Value() & 0xFF) << 0));
    }

    // Construct the Value object.
    IPC::Message bitmap_pickle;
    IPC::WriteParam(&bitmap_pickle, bitmap);
    *bitmap_value = base::BinaryValue::CreateWithCopiedBuffer(
        static_cast<const char*>(bitmap_pickle.data()), bitmap_pickle.size());

    return true;
  }

  // A special request for setting the extension action icon. This function
  // accepts a canvas ImageData object, so it needs to do extra processing
  // before sending the request to the browser.
  static v8::Handle<v8::Value> SetIconCommon(
      const v8::Arguments& args) {
    Value* bitmap_value = NULL;
    if (!ConvertImageDataToBitmapValue(args, &bitmap_value))
      return v8::Undefined();

    v8::Local<v8::Object> extension_args = args[1]->ToObject();
    v8::Local<v8::Object> details =
        extension_args->Get(v8::String::New("0"))->ToObject();

    DictionaryValue* dict = new DictionaryValue();
    dict->Set("imageData", bitmap_value);

    if (details->Has(v8::String::New("tabId"))) {
      dict->SetInteger("tabId",
                       details->Get(v8::String::New("tabId"))->Int32Value());
    }

    ListValue list_value;
    list_value.Append(dict);

    return StartRequestCommon(args, &list_value);
  }

  // Cached JS Array representation of each namespace in extension_api.json.
  // We store this so that we don't have to parse it over and over again for
  // every context that uses it.
  typedef std::map<std::string, v8::Persistent<v8::Object> > CachedSchemaMap;
  CachedSchemaMap schemas_;
};

}  // namespace

namespace extensions {

v8::Extension* SchemaGeneratedBindings::Get(
    ExtensionDispatcher* extension_dispatcher) {
  static v8::Extension* extension = new ExtensionImpl(extension_dispatcher);
  CHECK_EQ(extension_dispatcher,
           static_cast<ExtensionImpl*>(extension)->extension_dispatcher());
  return extension;
}

// static
void SchemaGeneratedBindings::HandleResponse(const ChromeV8ContextSet& contexts,
                                             int request_id,
                                             bool success,
                                             const std::string& response,
                                             const std::string& error,
                                             std::string* extension_id) {
  PendingRequestMap::iterator request =
      g_pending_requests.Get().find(request_id);
  if (request == g_pending_requests.Get().end()) {
    // This should not be able to happen since we only remove requests when they
    // are handled.
    LOG(ERROR) << "Could not find specified request id: " << request_id;
    return;
  }

  ChromeV8Context* v8_context =
      contexts.GetByV8Context(request->second->context);
  if (!v8_context)
    return;  // The frame went away.

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[5];
  argv[0] = v8::Integer::New(request_id);
  argv[1] = v8::String::New(request->second->name.c_str());
  argv[2] = v8::Boolean::New(success);
  argv[3] = v8::String::New(response.c_str());
  argv[4] = v8::String::New(error.c_str());

  v8::Handle<v8::Value> retval;
  CHECK(v8_context->CallChromeHiddenMethod("handleResponse",
                                           arraysize(argv),
                                           argv,
                                           &retval));
  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
#ifndef NDEBUG
  if (!retval.IsEmpty() && !retval->IsUndefined()) {
    std::string error = *v8::String::AsciiValue(retval);
    DCHECK(false) << error;
  }
#endif

  // Save the extension id before erasing the request.
  *extension_id = request->second->extension_id;

  request->second->context.Dispose();
  request->second->context.Clear();
  g_pending_requests.Get().erase(request);
}

// static
bool SchemaGeneratedBindings::HasPendingRequests(
    const std::string& extension_id) {
  for (PendingRequestMap::const_iterator it = g_pending_requests.Get().begin();
       it != g_pending_requests.Get().end(); ++it) {
    if (it->second->extension_id == extension_id)
      return true;
  }
  return false;
}

}  // namespace
