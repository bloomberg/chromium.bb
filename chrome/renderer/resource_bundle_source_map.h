// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
#define CHROME_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string_piece.h"
#include "chrome/renderer/extensions/module_system.h"
#include "chrome/renderer/static_v8_external_string_resource.h"
#include "v8/include/v8.h"

#include <map>
#include <string>

namespace ui {
  class ResourceBundle;
}

class ResourceBundleSourceMap : public extensions::ModuleSystem::SourceMap {
 public:
  explicit ResourceBundleSourceMap(const ui::ResourceBundle* resource_bundle);
  virtual ~ResourceBundleSourceMap();

  virtual v8::Handle<v8::Value> GetSource(const std::string& name) OVERRIDE;
  virtual bool Contains(const std::string& name) OVERRIDE;

  void RegisterSource(const std::string& name, int resource_id);

 private:
  v8::Handle<v8::String> ConvertString(const base::StringPiece& string);

  const ui::ResourceBundle* resource_bundle_;
  std::map<std::string, int> resource_id_map_;
};

#endif  // CHROME_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
