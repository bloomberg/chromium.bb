// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/external_extension.h"

#include <stdint.h>

#include "base/macros.h"
#include "chrome/common/search_provider.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

using blink::WebLocalFrame;
using blink::WebView;

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
void ExternalExtensionWrapper::IsSearchProviderInstalled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args.Length() || !args[0]->IsString())
    return;

  v8::String::Utf8Value utf8name(args[0]);
  if (!utf8name.length())
    return;

  std::string name(*utf8name);

  WebLocalFrame* webframe = WebLocalFrame::frameForCurrentContext();
  if (!webframe)
    return;

  chrome::mojom::InstallState install = chrome::mojom::InstallState::DENIED;
  GURL inquiry_url = GURL(webframe->document().url()).Resolve(name);
  if (!inquiry_url.is_empty()) {
    webframe->didCallIsSearchProviderInstalled();
    chrome::mojom::SearchProviderInstallStatePtr search_provider_service;
    content::RenderThread::Get()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&search_provider_service));
    if (!search_provider_service->GetInstallState(webframe->document().url(),
                                                  inquiry_url, &install)) {
      DLOG(ERROR) << "Can't fetch search provider install state";
    }
  }

  if (install == chrome::mojom::InstallState::DENIED) {
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
