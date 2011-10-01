// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_extension.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "content/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace {

const char kChromeHidden[] = "chromeHidden";

#ifndef NDEBUG
const char kValidateCallbacks[] = "validateCallbacks";
#endif

typedef std::map<int, std::string> StringMap;
static base::LazyInstance<StringMap> g_string_map(base::LINKER_INITIALIZED);

}  // namespace

// static
const char* ChromeV8Extension::GetStringResource(int resource_id) {
  StringMap* strings = g_string_map.Pointer();
  StringMap::iterator it = strings->find(resource_id);
  if (it == strings->end()) {
    it = strings->insert(std::make_pair(
        resource_id,
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            resource_id).as_string())).first;
  }
  return it->second.c_str();
}

// static
RenderView* ChromeV8Extension::GetCurrentRenderView() {
  WebFrame* webframe = WebFrame::frameForCurrentContext();
  DCHECK(webframe) << "RetrieveCurrentFrame called when not in a V8 context.";
  if (!webframe)
    return NULL;

  WebView* webview = webframe->view();
  if (!webview)
    return NULL;  // can happen during closing

  RenderView* renderview = RenderView::FromWebView(webview);
  DCHECK(renderview) << "Encountered a WebView without a WebViewDelegate";
  return renderview;
}

ChromeV8Extension::ChromeV8Extension(const char* name, int resource_id,
                                     ExtensionDispatcher* extension_dispatcher)
    : v8::Extension(name,
                    GetStringResource(resource_id),
                    0,  // num dependencies
                    NULL),  // dependencies array
      extension_dispatcher_(extension_dispatcher) {
}

ChromeV8Extension::ChromeV8Extension(const char* name, int resource_id,
                                     int dependency_count,
                                     const char** dependencies,
                                     ExtensionDispatcher* extension_dispatcher)
    : v8::Extension(name,
                    GetStringResource(resource_id),
                    dependency_count,
                    dependencies),
      extension_dispatcher_(extension_dispatcher) {
}

const Extension* ChromeV8Extension::GetExtensionForCurrentRenderView() const {
  RenderView* renderview = GetCurrentRenderView();
  if (!renderview)
    return NULL;  // this can happen as a tab is closing.

  GURL url = renderview->webview()->mainFrame()->document().url();
  const ExtensionSet* extensions = extension_dispatcher_->extensions();
  if (!extensions->ExtensionBindingsAllowed(url))
    return NULL;

  return extensions->GetByURL(url);
}

bool ChromeV8Extension::CheckPermissionForCurrentRenderView(
    const std::string& function_name) const {
  const ::Extension* extension = GetExtensionForCurrentRenderView();
  if (extension &&
      extension_dispatcher_->IsExtensionActive(extension->id()) &&
      extension->HasAPIPermission(function_name))
    return true;

  static const char kMessage[] =
      "You do not have permission to use '%s'. Be sure to declare"
      " in your manifest what permissions you need.";
  std::string error_msg = base::StringPrintf(kMessage, function_name.c_str());

  v8::ThrowException(v8::Exception::Error(v8::String::New(error_msg.c_str())));
  return false;
}

v8::Handle<v8::FunctionTemplate>
    ChromeV8Extension::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetChromeHidden"))) {
    return v8::FunctionTemplate::New(GetChromeHidden);
  }

  if (name->Equals(v8::String::New("Print"))) {
    return v8::FunctionTemplate::New(Print);
  }

  return v8::Handle<v8::FunctionTemplate>();
}

v8::Handle<v8::Value> ChromeV8Extension::GetChromeHidden(
    const v8::Arguments& args) {
  return GetChromeHidden(v8::Context::GetCurrent());
}

v8::Handle<v8::Value> ChromeV8Extension::GetChromeHidden(
    const v8::Handle<v8::Context>& context) {
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> hidden = global->GetHiddenValue(
      v8::String::New(kChromeHidden));

  if (hidden.IsEmpty() || hidden->IsUndefined()) {
    hidden = v8::Object::New();
    global->SetHiddenValue(v8::String::New(kChromeHidden), hidden);

#ifndef NDEBUG
    // Tell extension_process_bindings.js to validate callbacks and events
    // against their schema definitions in api/extension_api.json.
    v8::Local<v8::Object>::Cast(hidden)
        ->Set(v8::String::New(kValidateCallbacks), v8::True());
#endif
  }

  DCHECK(hidden->IsObject());
  return v8::Local<v8::Object>::Cast(hidden);
}

v8::Handle<v8::Value> ChromeV8Extension::Print(const v8::Arguments& args) {
  if (args.Length() < 1)
    return v8::Undefined();

  std::vector<std::string> components;
  for (int i = 0; i < args.Length(); ++i)
    components.push_back(*v8::String::Utf8Value(args[i]->ToString()));

  LOG(ERROR) << JoinString(components, ',');
  return v8::Undefined();
}
