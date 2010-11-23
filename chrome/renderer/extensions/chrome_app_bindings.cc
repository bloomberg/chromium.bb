// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_app_bindings.h"

#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;

namespace extensions_v8 {

static const char* const kAppExtensionName = "v8/ChromeApp";

class ChromeAppExtensionWrapper : public v8::Extension {
 public:
  ChromeAppExtensionWrapper() :
    v8::Extension(kAppExtensionName,
      "var chrome;"
      "if (!chrome)"
      "  chrome = {};"
      "if (!chrome.app) {"
      "  chrome.app = new function() {"
      "    native function GetIsInstalled();"
      "    this.__defineGetter__('isInstalled', GetIsInstalled);"
      "  };"
      "}") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetIsInstalled"))) {
      return v8::FunctionTemplate::New(GetIsInstalled);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> GetIsInstalled(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    if (!frame)
      return v8::Boolean::New(false);

    GURL url(frame->url());
    if (url.is_empty() ||
        !url.is_valid() ||
        !(url.SchemeIs("http") || url.SchemeIs("https")))
      return v8::Boolean::New(false);

    bool has_web_extent = (ExtensionRendererInfo::GetByURL(url) != NULL);
    return v8::Boolean::New(has_web_extent);
  }
};

v8::Extension* ChromeAppExtension::Get() {
  return new ChromeAppExtensionWrapper();
}

}  // namespace extensions_v8
