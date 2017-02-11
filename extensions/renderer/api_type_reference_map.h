// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_TYPE_REFERENCE_MAP_H_
#define EXTENSIONS_RENDERER_API_TYPE_REFERENCE_MAP_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"

namespace extensions {
class ArgumentSpec;

// A map from type name -> ArgumentSpec for API type definitions. This is used
// when an argument is declared to be a reference to a type defined elsewhere.
class APITypeReferenceMap {
 public:
  // A callback used to initialize an unknown type, so that these can be
  // created lazily.
  using InitializeTypeCallback = base::Callback<void(const std::string& name)>;

  explicit APITypeReferenceMap(const InitializeTypeCallback& initialize_type);
  ~APITypeReferenceMap();

  // Adds the |spec| to the map under the given |name|.
  void AddSpec(const std::string& name, std::unique_ptr<ArgumentSpec> spec);

  // Returns the spec for the given |name|.
  const ArgumentSpec* GetSpec(const std::string& name) const;

  bool empty() const { return type_refs_.empty(); }
  size_t size() const { return type_refs_.size(); }

 private:
  InitializeTypeCallback initialize_type_;

  std::map<std::string, std::unique_ptr<ArgumentSpec>> type_refs_;

  DISALLOW_COPY_AND_ASSIGN(APITypeReferenceMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_TYPE_REFERENCE_MAP_H_
