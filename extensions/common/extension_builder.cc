// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_builder.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

struct ExtensionBuilder::ManifestData {
  Type type;
  std::string name;
  std::vector<std::string> permissions;
  base::Optional<ActionType> action;
  std::unique_ptr<base::DictionaryValue> extra;

  std::unique_ptr<base::DictionaryValue> GetValue() const {
    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("manifest_version", 2)
        .Set("version", "0.1")
        .Set("description", "some description");

    switch (type) {
      case Type::EXTENSION:
        break;  // Sufficient already.
      case Type::PLATFORM_APP: {
        DictionaryBuilder background;
        background.Set("scripts", ListBuilder().Append("test.js").Build());
        manifest.Set(
            "app",
            DictionaryBuilder().Set("background", background.Build()).Build());
        break;
      }
    }

    if (!permissions.empty()) {
      ListBuilder permissions_builder;
      for (const std::string& permission : permissions)
        permissions_builder.Append(permission);
      manifest.Set("permissions", permissions_builder.Build());
    }

    if (action) {
      const char* action_key = nullptr;
      switch (*action) {
        case ActionType::PAGE_ACTION:
          action_key = manifest_keys::kPageAction;
          break;
        case ActionType::BROWSER_ACTION:
          action_key = manifest_keys::kBrowserAction;
          break;
      }
      manifest.Set(action_key, base::MakeUnique<base::DictionaryValue>());
    }

    std::unique_ptr<base::DictionaryValue> result = manifest.Build();
    if (extra)
      result->MergeDictionary(extra.get());

    return result;
  }
};

ExtensionBuilder::ExtensionBuilder()
    : location_(Manifest::UNPACKED), flags_(Extension::NO_FLAGS) {}

ExtensionBuilder::ExtensionBuilder(const std::string& name, Type type)
    : ExtensionBuilder() {
  manifest_data_ = base::MakeUnique<ManifestData>();
  manifest_data_->name = name;
  manifest_data_->type = type;
}

ExtensionBuilder::~ExtensionBuilder() {}

ExtensionBuilder::ExtensionBuilder(ExtensionBuilder&& other) = default;
ExtensionBuilder& ExtensionBuilder::operator=(ExtensionBuilder&& other) =
    default;

scoped_refptr<Extension> ExtensionBuilder::Build() {
  CHECK(manifest_data_ || manifest_value_);

  if (id_.empty() && manifest_data_)
    id_ = crx_file::id_util::GenerateId(manifest_data_->name);

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path_, location_,
      manifest_data_ ? *manifest_data_->GetValue() : *manifest_value_, flags_,
      id_, &error);

  CHECK(error.empty()) << error;
  CHECK(extension);

  return extension;
}

ExtensionBuilder& ExtensionBuilder::AddPermission(
    const std::string& permission) {
  CHECK(manifest_data_);
  manifest_data_->permissions.push_back(permission);
  return *this;
}

ExtensionBuilder& ExtensionBuilder::AddPermissions(
    const std::vector<std::string>& permissions) {
  CHECK(manifest_data_);
  manifest_data_->permissions.insert(manifest_data_->permissions.end(),
                                     permissions.begin(), permissions.end());
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetAction(ActionType action) {
  CHECK(manifest_data_);
  manifest_data_->action = action;
  return *this;
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
    std::unique_ptr<base::DictionaryValue> manifest) {
  CHECK(!manifest_data_);
  manifest_value_ = std::move(manifest);
  return *this;
}

ExtensionBuilder& ExtensionBuilder::MergeManifest(
    std::unique_ptr<base::DictionaryValue> manifest) {
  if (manifest_data_) {
    if (!manifest_data_->extra)
      manifest_data_->extra = base::MakeUnique<base::DictionaryValue>();
    manifest_data_->extra->MergeDictionary(manifest.get());
  } else {
    manifest_value_->MergeDictionary(manifest.get());
  }
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
