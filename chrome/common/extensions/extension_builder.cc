// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_builder.h"

#include "chrome/common/extensions/extension.h"

namespace extensions {

ExtensionBuilder::ExtensionBuilder()
    : location_(Manifest::UNPACKED),
      flags_(Extension::NO_FLAGS) {
}
ExtensionBuilder::~ExtensionBuilder() {}

scoped_refptr<Extension> ExtensionBuilder::Build() {
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path_,
      location_,
      *manifest_,
      flags_,
      id_,
      &error);
  CHECK_EQ("", error);
  return extension;
}

ExtensionBuilder& ExtensionBuilder::SetPath(const base::FilePath& path) {
  path_ = path;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetLocation(Manifest::Location location) {
  location_ = location;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetManifest(
    scoped_ptr<DictionaryValue> manifest) {
  manifest_ = manifest.Pass();
  return *this;
}

ExtensionBuilder& ExtensionBuilder::AddFlags(int init_from_value_flags) {
  flags_ |= init_from_value_flags;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetID(const std::string& id) {
  id_ = id;
  return *this;
}

}  // namespace extensions
