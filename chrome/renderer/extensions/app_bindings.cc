// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_bindings.h"

#include "base/command_line.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using content::V8ValueConverter;

namespace {

bool IsCheckoutURL(const std::string& url_spec) {
  std::string checkout_url_prefix =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAppsCheckoutURL);
  if (checkout_url_prefix.empty())
    checkout_url_prefix = "https://checkout.google.com/";

  return StartsWithASCII(url_spec, checkout_url_prefix, false);
}

bool CheckAccessToAppDetails(WebFrame* frame) {
  if (!IsCheckoutURL(frame->document().url().spec())) {
    std::string error("Access denied for URL: ");
    error += frame->document().url().spec();
    v8::ThrowException(v8::String::New(error.c_str()));
    return false;
  }

  return true;
}

const char* kMissingClientIdError = "Missing clientId parameter";
const char* kInvalidClientIdError = "Invalid clientId";
const char* kInvalidCallbackIdError = "Invalid callbackId";


class AppBindingsHandler : public ChromeV8ExtensionHandler {
 public:
  AppBindingsHandler(ExtensionDispatcher* dispatcher, ChromeV8Context* context);

  // ChromeV8ExtensionHandler
  virtual v8::Handle<v8::Value> HandleNativeFunction(
      const std::string& name,
      const v8::Arguments& arguments) OVERRIDE;

  // IPC::Channel::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  v8::Handle<v8::Value> GetIsInstalled(const v8::Arguments& args);
  v8::Handle<v8::Value> Install(const v8::Arguments& args);
  v8::Handle<v8::Value> GetDetails(const v8::Arguments& args);
  v8::Handle<v8::Value> GetDetailsForFrame(const v8::Arguments& args);
  v8::Handle<v8::Value> GetAppNotifyChannel(const v8::Arguments& args);

  v8::Handle<v8::Value> GetDetailsForFrameImpl(WebKit::WebFrame* frame);

  void OnGetAppNotifyChannelResponse(const std::string& channel_id,
                                     const std::string& error,
                                     int callback_id);

  ExtensionDispatcher* dispatcher_;
  DISALLOW_COPY_AND_ASSIGN(AppBindingsHandler);
};

}  // namespace


AppBindings::AppBindings(ExtensionDispatcher* dispatcher)
    : ChromeV8Extension("extensions/app.js", IDR_APP_BINDINGS_JS,
                        dispatcher) {
}

ChromeV8ExtensionHandler* AppBindings::CreateHandler(
    ChromeV8Context* context) {
  return new AppBindingsHandler(extension_dispatcher(), context);
}



AppBindingsHandler::AppBindingsHandler(ExtensionDispatcher* dispatcher,
                                       ChromeV8Context* context)
    : ChromeV8ExtensionHandler(context),
      dispatcher_(dispatcher) {
}

v8::Handle<v8::Value> AppBindingsHandler::HandleNativeFunction(
    const std::string& name, const v8::Arguments& args) {
  // TODO(aa): Create a helper map of callback that can be used in either
  // extensions or handlers.
  if (name == "GetIsInstalled") {
    return GetIsInstalled(args);
  } else if (name == "Install") {
    return Install(args);
  } else if (name == "GetDetails") {
    return GetDetails(args);
  } else if (name == "GetDetailsForFrame") {
    return GetDetailsForFrame(args);
  } else if (name == "GetAppNotifyChannel") {
    return GetAppNotifyChannel(args);
  } else {
    CHECK(false) << "Unknown native function: " << name;
  }

  return v8::Undefined();
}

v8::Handle<v8::Value> AppBindingsHandler::GetIsInstalled(
    const v8::Arguments& args) {
  // TODO(aa): Hm, maybe ExtensionBindingsContext should have GetExtension()
  // afterall?
  const ::Extension* extension =
      dispatcher_->extensions()->GetByID(context_->extension_id());

  // TODO(aa): Why only hosted app?
  // TODO(aa): GARRR - why is there IsExtensionActive and IsApplicationActive!?
  bool result = extension && extension->is_hosted_app() &&
      dispatcher_->IsApplicationActive(extension->id());
  return v8::Boolean::New(result);
}

v8::Handle<v8::Value> AppBindingsHandler::Install(const v8::Arguments& args) {
  content::RenderView* render_view = context_->GetRenderView();
  CHECK(render_view);

  string16 error;
  ExtensionHelper* helper = ExtensionHelper::Get(render_view);
  if (!helper->InstallWebApplicationUsingDefinitionFile(
          context_->web_frame(), &error)) {
    v8::ThrowException(v8::String::New(UTF16ToUTF8(error).c_str()));
  }

  return v8::Undefined();
}

v8::Handle<v8::Value> AppBindingsHandler::GetDetails(
    const v8::Arguments& args) {
  CHECK(context_->web_frame());
  return GetDetailsForFrameImpl(context_->web_frame());
}

v8::Handle<v8::Value> AppBindingsHandler::GetDetailsForFrame(
    const v8::Arguments& args) {
  CHECK(context_->web_frame());
  if (!CheckAccessToAppDetails(context_->web_frame()))
    return v8::Undefined();

  if (args.Length() < 0)
    return v8::ThrowException(v8::String::New("Not enough arguments."));

  if (!args[0]->IsObject()) {
    return v8::ThrowException(
        v8::String::New("Argument 0 must be an object."));
  }

  v8::Local<v8::Context> context =
      v8::Local<v8::Object>::Cast(args[0])->CreationContext();
  CHECK(!context.IsEmpty());

  WebFrame* target_frame = WebFrame::frameForContext(context);
  if (!target_frame) {
    return v8::ThrowException(
        v8::String::New("Could not find frame for specified object."));
  }

  return GetDetailsForFrameImpl(target_frame);
}

v8::Handle<v8::Value> AppBindingsHandler::GetDetailsForFrameImpl(
    WebFrame* frame) {
  const ::Extension* extension = dispatcher_->extensions()->GetByURL(
      ExtensionURLInfo(frame->document().securityOrigin(),
                       frame->document().url()));
  if (!extension)
    return v8::Null();

  scoped_ptr<DictionaryValue> manifest_copy(
      extension->manifest_value()->DeepCopy());
  manifest_copy->SetString("id", extension->id());
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  return converter->ToV8Value(manifest_copy.get(),
                              frame->mainWorldScriptContext());
}

v8::Handle<v8::Value> AppBindingsHandler::GetAppNotifyChannel(
    const v8::Arguments& args) {
  // Read the required 'clientId' value out of the object at args[0].
  std::string client_id;
  if (args.Length() < 1 || !args[0]->IsObject()) {
    v8::ThrowException(v8::String::New(kMissingClientIdError));
    return v8::Undefined();
  }
  v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(args[0]);
  v8::Local<v8::String> client_id_key = v8::String::New("clientId");
  if (obj->Has(client_id_key)) {
    v8::String::Utf8Value id_value(obj->Get(client_id_key));
    if (id_value.length() > 0)
      client_id = std::string(*id_value);
  }
  if (client_id.empty()) {
    v8::ThrowException(v8::String::New(kInvalidClientIdError));
    return v8::Undefined();
  }

  // Get the callbackId if specified.
  int callback_id = 0;
  if (args.Length() > 1) {
    if (!args[1]->IsInt32()) {
      v8::ThrowException(v8::String::New(kInvalidCallbackIdError));
      return v8::Undefined();
    }
    callback_id = args[1]->Int32Value();
  }

  content::RenderView* render_view = context_->GetRenderView();
  CHECK(render_view);

  Send(new ExtensionHostMsg_GetAppNotifyChannel(
      render_view->GetRoutingId(), context_->web_frame()->document().url(),
      client_id, GetRoutingId(), callback_id));
  return v8::Undefined();
}

bool AppBindingsHandler::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AppBindingsHandler, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_GetAppNotifyChannelResponse,
                        OnGetAppNotifyChannelResponse)
    IPC_MESSAGE_UNHANDLED(CHECK(false) << "Unhandled IPC message")
  IPC_END_MESSAGE_MAP()
  return true;
}

void AppBindingsHandler::OnGetAppNotifyChannelResponse(
    const std::string& channel_id, const std::string& error, int callback_id) {
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(context_->v8_context());
  v8::Handle<v8::Value> argv[3];
  argv[0] = v8::String::New(channel_id.c_str());
  argv[1] = v8::String::New(error.c_str());
  argv[2] = v8::Integer::New(callback_id);
  CHECK(context_->CallChromeHiddenMethod("app.onGetAppNotifyChannelResponse",
                                         arraysize(argv), argv, NULL));
}
