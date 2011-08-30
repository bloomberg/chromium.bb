// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_base.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/bindings_utils.h"
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

typedef std::map<int, std::string> StringMap;
static base::LazyInstance<StringMap> g_string_map(base::LINKER_INITIALIZED);

}

// static
const char* ExtensionBase::GetStringResource(int resource_id) {
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

const Extension* ExtensionBase::GetExtensionForCurrentContext() const {
  RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
  if (!renderview)
    return NULL;  // this can happen as a tab is closing.

  GURL url = renderview->webview()->mainFrame()->document().url();
  const ExtensionSet* extensions = extension_dispatcher_->extensions();
  if (!extensions->ExtensionBindingsAllowed(url))
    return NULL;

  return extensions->GetByURL(url);
}

bool ExtensionBase::CheckPermissionForCurrentContext(
    const std::string& function_name) const {
  const ::Extension* extension = GetExtensionForCurrentContext();
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
    ExtensionBase::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetChromeHidden"))) {
    return v8::FunctionTemplate::New(GetChromeHidden);
  }

  if (name->Equals(v8::String::New("Print"))) {
    return v8::FunctionTemplate::New(Print);
  }

  return v8::Handle<v8::FunctionTemplate>();
}

v8::Handle<v8::Value> ExtensionBase::GetChromeHidden(
    const v8::Arguments& args) {
  return bindings_utils::GetChromeHiddenForContext(v8::Context::GetCurrent());
}

v8::Handle<v8::Value> ExtensionBase::Print(const v8::Arguments& args) {
  if (args.Length() < 1)
    return v8::Undefined();

  std::vector<std::string> components;
  for (int i = 0; i < args.Length(); ++i)
    components.push_back(*v8::String::Utf8Value(args[i]->ToString()));

  LOG(ERROR) << JoinString(components, ',');
  return v8::Undefined();
}
