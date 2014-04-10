// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/principals_extension_bindings.h"

#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using blink::WebLocalFrame;
using blink::WebView;
using content::RenderView;

namespace {

class PrincipalsExtensionWrapper : public v8::Extension {
 public:
  PrincipalsExtensionWrapper();

 private:
  // v8::Extension overrides.
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE;

  static RenderView* GetRenderView();

  static void GetManagedAccounts(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ShowBrowserAccountManagementUI(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(PrincipalsExtensionWrapper);
};

const char* kPrincipalsExtensionName = "v8/Principals";
const char* kPrincipalsExtensionCode =
    "var chrome;"
    "if (!chrome)"
    "  chrome = {};"
    "if (!chrome.principals)"
    "  chrome.principals = {};"
    "chrome.principals.getManagedAccounts = function() {"
    "  native function NativeGetManagedAccounts();"
    "  return NativeGetManagedAccounts();"
    "};"
    "chrome.principals.showBrowserAccountManagementUI = function() {"
    "  native function ShowBrowserAccountManagementUI();"
    "  ShowBrowserAccountManagementUI();"
    "};";

PrincipalsExtensionWrapper::PrincipalsExtensionWrapper() : v8::Extension(
    kPrincipalsExtensionName, kPrincipalsExtensionCode) {}

v8::Handle<v8::FunctionTemplate>
    PrincipalsExtensionWrapper::GetNativeFunctionTemplate(
        v8::Isolate* isolate, v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::NewFromUtf8(
      isolate, "NativeGetManagedAccounts"))) {
    return v8::FunctionTemplate::New(isolate, GetManagedAccounts);
  } else if (name->Equals(v8::String::NewFromUtf8(
      isolate, "ShowBrowserAccountManagementUI"))) {
    return v8::FunctionTemplate::New(isolate, ShowBrowserAccountManagementUI);
  }
  return v8::Handle<v8::FunctionTemplate>();
}

void PrincipalsExtensionWrapper::GetManagedAccounts(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) {
    args.GetReturnValue().SetNull();
    return;
  }

  std::vector<std::string> accounts;
  render_view->Send(new ChromeViewHostMsg_GetManagedAccounts(
      WebLocalFrame::frameForCurrentContext()->document().url(),
      &accounts));

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Array> v8_result = v8::Array::New(isolate);
  int v8_index = 0;
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    v8_result->Set(v8::Integer::New(isolate, v8_index++),
        v8::String::NewFromUtf8(isolate, it->c_str(),
                                v8::String::kNormalString, it->length()));
  }
  args.GetReturnValue().Set(v8_result);
}

void PrincipalsExtensionWrapper::ShowBrowserAccountManagementUI(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(guohui): need to figure out how to prevent abuse.
  RenderView* render_view = GetRenderView();
  if (!render_view) return;

  render_view->Send(new ChromeViewHostMsg_ShowBrowserAccountManagementUI());
}

RenderView* PrincipalsExtensionWrapper::GetRenderView() {
  WebLocalFrame* webframe = WebLocalFrame::frameForCurrentContext();
  DCHECK(webframe) << "There should be an active frame since we just got "
      "a native function called.";
  if (!webframe)
    return NULL;

  WebView* webview = webframe->view();
  if (!webview)  // can happen during closing
    return NULL;

  return RenderView::FromWebView(webview);
}

}  // namespace

namespace extensions_v8 {

v8::Extension* PrincipalsExtension::Get() {
  return new PrincipalsExtensionWrapper();
}

}  // namespace extensions_v8
