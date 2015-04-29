// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/resource_bundle_source_map.h"

#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

ResourceBundleSourceMap::ResourceBundleSourceMap(
    const ui::ResourceBundle* resource_bundle)
    : resource_bundle_(resource_bundle) {
}

ResourceBundleSourceMap::~ResourceBundleSourceMap() {
}

void ResourceBundleSourceMap::RegisterSource(const std::string& name,
                                             int resource_id) {
  resource_id_map_[name] = resource_id;
}

v8::Local<v8::Value> ResourceBundleSourceMap::GetSource(
    v8::Isolate* isolate,
    const std::string& name) {
  if (!Contains(name)) {
    NOTREACHED() << "No module is registered with name \"" << name << "\"";
    return v8::Undefined(isolate);
  }
  base::StringPiece resource =
      resource_bundle_->GetRawDataResource(resource_id_map_[name]);
  if (resource.empty()) {
    NOTREACHED()
        << "Module resource registered as \"" << name << "\" not found";
    return v8::Undefined(isolate);
  }
  return ConvertString(isolate, resource);
}

bool ResourceBundleSourceMap::Contains(const std::string& name) {
  return !!resource_id_map_.count(name);
}

v8::Local<v8::String> ResourceBundleSourceMap::ConvertString(
    v8::Isolate* isolate,
    const base::StringPiece& string) {
  // v8 takes ownership of the StaticV8ExternalOneByteStringResource (see
  // v8::String::NewExternal()).
  return v8::String::NewExternal(
      isolate, new StaticV8ExternalOneByteStringResource(string));
}

}  // namespace extensions
