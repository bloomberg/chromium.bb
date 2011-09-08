// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_app_bindings.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/renderer/render_view.h"
#include "content/renderer/v8_value_converter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;

namespace {

bool IsCheckoutURL(const std::string& url_spec) {
  std::string checkout_url_prefix =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAppsCheckoutURL);
  if (checkout_url_prefix.empty())
    checkout_url_prefix = "https://checkout.google.com/";

  return StartsWithASCII(url_spec, checkout_url_prefix, false);
}

bool CheckAccessToAppDetails() {
  WebFrame* frame = WebFrame::frameForCurrentContext();
  if (!frame) {
    LOG(ERROR) << "Could not get frame for current context.";
    return false;
  }

  if (!IsCheckoutURL(frame->document().url().spec())) {
    std::string error("Access denied for URL: ");
    error += frame->document().url().spec();
    v8::ThrowException(v8::String::New(error.c_str()));
    return false;
  }

  return true;
}

}

namespace extensions_v8 {

static const char* const kAppExtensionName = "v8/ChromeApp";

class ChromeAppExtensionWrapper : public v8::Extension {
 public:
  explicit ChromeAppExtensionWrapper(ExtensionDispatcher* extension_dispatcher) :
    v8::Extension(kAppExtensionName,
      "var chrome;"
      "if (!chrome)"
      "  chrome = {};"
      "if (!chrome.app) {"
      "  chrome.app = new function() {"
      "    native function GetIsInstalled();"
      "    native function Install();"
      "    native function GetDetails();"
      "    native function GetDetailsForFrame();"
      "    this.__defineGetter__('isInstalled', GetIsInstalled);"
      "    this.install = Install;"
      "    this.getDetails = GetDetails;"
      "    this.getDetailsForFrame = GetDetailsForFrame;"
      "  };"
      "}") {
    extension_dispatcher_ = extension_dispatcher;
  }

  ~ChromeAppExtensionWrapper() {
    extension_dispatcher_ = NULL;
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetIsInstalled"))) {
      return v8::FunctionTemplate::New(GetIsInstalled);
    } else if (name->Equals(v8::String::New("Install"))) {
      return v8::FunctionTemplate::New(Install);
    } else if (name->Equals(v8::String::New("GetDetails"))) {
      return v8::FunctionTemplate::New(GetDetails);
    } else if (name->Equals(v8::String::New("GetDetailsForFrame"))) {
      return v8::FunctionTemplate::New(GetDetailsForFrame);
    } else {
      return v8::Handle<v8::FunctionTemplate>();
    }
  }

  static v8::Handle<v8::Value> GetIsInstalled(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    if (!frame)
      return v8::False();

    GURL url(frame->document().url());
    if (url.is_empty() ||
        !url.is_valid() ||
        !(url.SchemeIs("http") || url.SchemeIs("https")))
      return v8::False();

    const ::Extension* extension =
        extension_dispatcher_->extensions()->GetByURL(frame->document().url());

    bool has_web_extent = extension &&
        extension_dispatcher_->IsApplicationActive(extension->id());
    return v8::Boolean::New(has_web_extent);
  }

  static v8::Handle<v8::Value> Install(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    if (!frame || !frame->view())
      return v8::Undefined();

    RenderView* render_view = RenderView::FromWebView(frame->view());
    if (!render_view)
      return v8::Undefined();

    string16 error;
    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    if (!helper->InstallWebApplicationUsingDefinitionFile(frame, &error))
      v8::ThrowException(v8::String::New(UTF16ToUTF8(error).c_str()));

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetDetails(const v8::Arguments& args) {
    return GetDetailsForFrameImpl(WebFrame::frameForCurrentContext());
  }

  static v8::Handle<v8::Value> GetDetailsForFrame(
      const v8::Arguments& args) {
    if (!CheckAccessToAppDetails())
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

  static v8::Handle<v8::Value> GetDetailsForFrameImpl(const WebFrame* frame) {
    const ::Extension* extension =
        extension_dispatcher_->extensions()->GetByURL(frame->document().url());
    if (!extension)
      return v8::Null();

    scoped_ptr<DictionaryValue> manifest_copy(
        extension->manifest_value()->DeepCopy());
    manifest_copy->SetString("id", extension->id());
    V8ValueConverter converter;
    return converter.ToV8Value(manifest_copy.get(),
                               frame->mainWorldScriptContext());
  }

  static ExtensionDispatcher* extension_dispatcher_;
};

ExtensionDispatcher* ChromeAppExtensionWrapper::extension_dispatcher_;

v8::Extension* ChromeAppExtension::Get(
    ExtensionDispatcher* extension_dispatcher) {
  return new ChromeAppExtensionWrapper(extension_dispatcher);
}

}  // namespace extensions_v8
