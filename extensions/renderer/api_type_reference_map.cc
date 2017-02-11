// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_type_reference_map.h"

#include "extensions/renderer/argument_spec.h"

namespace extensions {

APITypeReferenceMap::APITypeReferenceMap(
    const InitializeTypeCallback& initialize_type)
    : initialize_type_(initialize_type) {}
APITypeReferenceMap::~APITypeReferenceMap() = default;

void APITypeReferenceMap::AddSpec(const std::string& name,
                                  std::unique_ptr<ArgumentSpec> spec) {
  DCHECK(type_refs_.find(name) == type_refs_.end());
  type_refs_[name] = std::move(spec);
}

const ArgumentSpec* APITypeReferenceMap::GetSpec(
    const std::string& name) const {
  auto iter = type_refs_.find(name);
  if (iter == type_refs_.end()) {
    initialize_type_.Run(name);
    iter = type_refs_.find(name);
  }
  return iter == type_refs_.end() ? nullptr : iter->second.get();
}

}  // namespace extensions
