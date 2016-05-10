// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_extension.h"

#include <stdint.h>

#include "base/macros.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/search_provider.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using blink::WebLocalFrame;
using blink::WebView;
using content::RenderView;

namespace extensions_v8 {

namespace {

const char* const kSearchProviderApi =
    "var external;"
    "if (!external)"
    "  external = {};"
    "external.IsSearchProviderInstalled = function(name) {"
    "  native function NativeIsSearchProviderInstalled();"
    "  return NativeIsSearchProviderInstalled(name);"
    "};";

const char kExternalExtensionName[] = "v8/External";

}  // namespace

class ExternalExtensionWrapper : public v8::Extension {
 public:
  ExternalExtensionWrapper();

  // Allows v8's javascript code to call the native functions defined
  // in this class for window.external.
  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::String> name) override;

  // Helper function to find the RenderView. May return NULL.
  static RenderView* GetRenderView();

  // Implementation of window.external.IsSearchProviderInstalled.
  static void IsSearchProviderInstalled(
      const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalExtensionWrapper);
};

ExternalExtensionWrapper::ExternalExtensionWrapper()
    : v8::Extension(kExternalExtensionName, kSearchProviderApi) {
}

v8::Local<v8::FunctionTemplate>
ExternalExtensionWrapper::GetNativeFunctionTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::String> name) {
  if (name->Equals(v8::String::NewFromUtf8(
          isolate, "NativeIsSearchProviderInstalled"))) {
    return v8::FunctionTemplate::New(isolate, IsSearchProviderInstalled);
  }

  return v8::Local<v8::FunctionTemplate>();
}

// static
RenderView* ExternalExtensionWrapper::GetRenderView() {
  WebLocalFrame* webframe = WebLocalFrame::frameForCurrentContext();
  DCHECK(webframe) << "There should be an active frame since we just got "
      "a native function called.";
  if (!webframe)
    return NULL;

  WebView* webview = webframe->view();
  if (!webview)
    return NULL;  // can happen during closing

  return RenderView::FromWebView(webview);
}

// static
void ExternalExtensionWrapper::IsSearchProviderInstalled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args.Length() || !args[0]->IsString())
    return;

  v8::String::Utf8Value utf8name(args[0]);
  if (!utf8name.length())
    return;

  std::string name(*utf8name);
  RenderView* render_view = GetRenderView();
  if (!render_view)
    return;

  WebLocalFrame* webframe = WebLocalFrame::frameForCurrentContext();
  if (!webframe)
    return;

  search_provider::InstallState install = search_provider::DENIED;
  GURL inquiry_url = GURL(webframe->document().url()).Resolve(name);
  if (!inquiry_url.is_empty()) {
    webframe->didCallIsSearchProviderInstalled();
    render_view->Send(new ChromeViewHostMsg_GetSearchProviderInstallState(
        render_view->GetRoutingID(), webframe->document().url(), inquiry_url,
        &install));
  }

  if (install == search_provider::DENIED) {
    // FIXME: throw access denied exception.
    v8::Isolate* isolate = args.GetIsolate();
    isolate->ThrowException(v8::Exception::Error(v8::String::Empty(isolate)));
    return;
  }
  args.GetReturnValue().Set(static_cast<int32_t>(install));
}

v8::Extension* ExternalExtension::Get() {
  return new ExternalExtensionWrapper();
}

}  // namespace extensions_v8
