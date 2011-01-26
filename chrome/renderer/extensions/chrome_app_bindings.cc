// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_app_bindings.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
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
      "    native function Install();"
      "    this.__defineGetter__('isInstalled', GetIsInstalled);"
      "    this.install = Install;"
      "  };"
      "}") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetIsInstalled"))) {
      return v8::FunctionTemplate::New(GetIsInstalled);
    } else if (name->Equals(v8::String::New("Install"))) {
      return v8::FunctionTemplate::New(Install);
    } else {
      return v8::Handle<v8::FunctionTemplate>();
    }
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

    bool has_web_extent =
        RenderThread::current()->GetExtensions()->GetByURL(url) != NULL;
    return v8::Boolean::New(has_web_extent);
  }

  static v8::Handle<v8::Value> Install(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    RenderView* render_view = bindings_utils::GetRenderViewForCurrentContext();
    if (frame && render_view) {
      string16 error;
      if (!render_view->InstallWebApplicationUsingDefinitionFile(frame, &error))
        v8::ThrowException(v8::String::New(UTF16ToUTF8(error).c_str()));
    }

    return v8::Undefined();
  }
};

v8::Extension* ChromeAppExtension::Get() {
  return new ChromeAppExtensionWrapper();
}

}  // namespace extensions_v8
