// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_extension.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace extensions_v8 {

static const char* const kSearchProviderApiV1 =
    "var external;"
    "if (!external)"
    "  external = {};"
    "external.AddSearchProvider = function(name) {"
    "  native function NativeAddSearchProvider();"
    "  NativeAddSearchProvider(name);"
    "};";

static const char* const kSearchProviderApiV2 =
    "var external;"
    "if (!external)"
    "  external = {};"
    "external.AddSearchProvider = function(name, default_provider) {"
    "  native function NativeAddSearchProvider();"
    "  NativeAddSearchProvider(name, default_provider);"
    "};"
    "external.IsSearchProviderInstalled = function(name) {"
    "  native function NativeIsSearchProviderInstalled();"
    "  return NativeIsSearchProviderInstalled(name);"
    "};";

#undef SEARCH_PROVIDER_API_V1

const char* const kExternalExtensionName = "v8/External";

// Should the new api's "IsSearchProviderInstalled and InstallSearchProvider
// with an extra parameter to indicate if the provider should be the default"
// be available?
static bool EnableSearchProviderV2() {
#if defined(OS_WIN)
  return true;
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSearchProviderApiV2);
#endif
}

class ExternalExtensionWrapper : public v8::Extension {
 public:
  ExternalExtensionWrapper();

  // Allows v8's javascript code to call the native functions defined
  // in this class for window.external.
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name);

  // Helper function to find the RenderView. May return NULL.
  static RenderView* GetRenderView();

  // Implementation of window.external.AddSearchProvider.
  static v8::Handle<v8::Value> AddSearchProvider(const v8::Arguments& args);

  // Implementation of window.external.IsSearchProviderInstalled.
  static v8::Handle<v8::Value> IsSearchProviderInstalled(
      const v8::Arguments& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalExtensionWrapper);
};

ExternalExtensionWrapper::ExternalExtensionWrapper()
    : v8::Extension(
        kExternalExtensionName,
        !EnableSearchProviderV2() ?
        kSearchProviderApiV1 : kSearchProviderApiV2) {
}

v8::Handle<v8::FunctionTemplate> ExternalExtensionWrapper::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("NativeAddSearchProvider")))
    return v8::FunctionTemplate::New(AddSearchProvider);

  if (name->Equals(v8::String::New("NativeIsSearchProviderInstalled")))
    return v8::FunctionTemplate::New(IsSearchProviderInstalled);

  return v8::Handle<v8::FunctionTemplate>();
}

// static
RenderView* ExternalExtensionWrapper::GetRenderView() {
  WebFrame* webframe = WebFrame::frameForEnteredContext();
  DCHECK(webframe) << "There should be an active frame since we just got "
      "a native function called.";
  if (!webframe) return NULL;

  WebView* webview = webframe->view();
  if (!webview) return NULL;  // can happen during closing

  return RenderView::FromWebView(webview);
}

// static
v8::Handle<v8::Value> ExternalExtensionWrapper::AddSearchProvider(
    const v8::Arguments& args) {
  if (!args.Length()) return v8::Undefined();

  std::string name = std::string(*v8::String::Utf8Value(args[0]));
  if (!name.length()) return v8::Undefined();

  ViewHostMsg_PageHasOSDD_Type provider_type =
      ((args.Length() < 2) || !args[1]->BooleanValue()) ?
      ViewHostMsg_PageHasOSDD_Type::Explicit() :
      ViewHostMsg_PageHasOSDD_Type::ExplicitDefault();

  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  render_view->AddSearchProvider(name, provider_type);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> ExternalExtensionWrapper::IsSearchProviderInstalled(
    const v8::Arguments& args) {
  if (!args.Length()) return v8::Undefined();

  std::string name = std::string(*v8::String::Utf8Value(args[0]));
  if (!name.length()) return v8::Undefined();

  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  WebFrame* webframe = WebFrame::frameForEnteredContext();
  if (!webframe) return v8::Undefined();

  ViewHostMsg_GetSearchProviderInstallState_Params install
      = render_view->GetSearchProviderInstallState(webframe, name);
  if (install.state ==
      ViewHostMsg_GetSearchProviderInstallState_Params::DENIED) {
    // FIXME: throw access denied exception.
    return v8::ThrowException(v8::Exception::Error(v8::String::Empty()));
  }
  return v8::Integer::New(install.state);
}

v8::Extension* ExternalExtension::Get() {
  return new ExternalExtensionWrapper();
}

}  // namespace extensions_v8
