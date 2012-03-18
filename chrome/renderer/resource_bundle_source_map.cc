// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/resource_bundle_source_map.h"

#include "ui/base/resource/resource_bundle.h"

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

v8::Handle<v8::Value> ResourceBundleSourceMap::GetSource(
    const std::string& name) {
  if (!Contains(name))
    return v8::Undefined();
  int resource_id = resource_id_map_[name];
  return ConvertString(resource_bundle_->GetRawDataResource(resource_id));
}

bool ResourceBundleSourceMap::Contains(const std::string& name) {
  return !!resource_id_map_.count(name);
}

v8::Handle<v8::String> ResourceBundleSourceMap::ConvertString(
    const base::StringPiece& string) {
  // v8 takes ownership of the StaticV8ExternalAsciiStringResource (see
  // v8::String::NewExternal()).
  return v8::String::NewExternal(
      new StaticV8ExternalAsciiStringResource(string));
}
