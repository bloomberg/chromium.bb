// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/policy/policy_schema.h"
#include "extensions/common/install_warning.h"

using extension_manifest_keys::kStorageManagedSchema;

namespace extensions {

StorageSchemaManifestHandler::StorageSchemaManifestHandler() {}

StorageSchemaManifestHandler::~StorageSchemaManifestHandler() {}

// static
scoped_ptr<policy::PolicySchema> StorageSchemaManifestHandler::GetSchema(
    const Extension* extension,
    std::string* error) {
  if (!extension->HasAPIPermission(APIPermission::kStorage)) {
    *error = base::StringPrintf("The storage permission is required to use %s",
                                kStorageManagedSchema);
    return scoped_ptr<policy::PolicySchema>();
  }
  std::string path;
  extension->manifest()->GetString(kStorageManagedSchema, &path);
  base::FilePath file = base::FilePath::FromUTF8Unsafe(path);
  if (file.IsAbsolute() || file.ReferencesParent()) {
    *error = base::StringPrintf("%s must be a relative path without ..",
                                kStorageManagedSchema);
    return scoped_ptr<policy::PolicySchema>();
  }
  file = extension->path().AppendASCII(path);
  if (!base::PathExists(file)) {
    *error =
        base::StringPrintf("File does not exist: %s", file.value().c_str());
    return scoped_ptr<policy::PolicySchema>();
  }
  std::string content;
  if (!file_util::ReadFileToString(file, &content)) {
    *error = base::StringPrintf("Can't read %s", file.value().c_str());
    return scoped_ptr<policy::PolicySchema>();
  }
  return policy::PolicySchema::Parse(content, error);
}

bool StorageSchemaManifestHandler::Parse(Extension* extension,
                                         string16* error) {
  std::string path;
  if (!extension->manifest()->GetString(kStorageManagedSchema, &path)) {
    *error = ASCIIToUTF16(
        base::StringPrintf("%s must be a string", kStorageManagedSchema));
    return false;
  }
  return true;
}

bool StorageSchemaManifestHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  return !!GetSchema(extension, error);
}

const std::vector<std::string> StorageSchemaManifestHandler::Keys() const {
  return SingleKey(kStorageManagedSchema);
}

}  // namespace extensions
