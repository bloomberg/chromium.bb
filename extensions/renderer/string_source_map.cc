// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/string_source_map.h"

#include "gin/converter.h"

namespace extensions {

StringSourceMap::StringSourceMap() {}
StringSourceMap::~StringSourceMap() {}

v8::Local<v8::String> StringSourceMap::GetSource(
    v8::Isolate* isolate,
    const std::string& name) const {
  const auto& iter = sources_.find(name);
  if (iter == sources_.end())
    return v8::Local<v8::String>();
  return gin::StringToV8(isolate, iter->second);
}

bool StringSourceMap::Contains(const std::string& name) const {
  return sources_.find(name) != sources_.end();
}

void StringSourceMap::RegisterModule(const std::string& name,
                                     const std::string& source) {
  CHECK_EQ(0u, sources_.count(name)) << "A module for '" << name
                                     << "' already exists.";
  sources_[name] = source;
}

}  // namespace extensions
