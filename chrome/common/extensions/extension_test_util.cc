// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_test_util.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;
using extensions::Manifest;

namespace extensions {
namespace extension_test_util {

ExtensionBuilder& BuildExtension(ExtensionBuilder& builder) {
  return builder
         .SetManifest(DictionaryBuilder()
                      .Set("name", "Test extension")
                      .Set("version", "1.0")
                      .Set("manifest_version", 2));
}

ExtensionBuilder& BuildExtensionWithPermissions(ExtensionBuilder& builder,
                                                ListBuilder& permissions) {
  return
      BuildExtension(builder)
      .MergeManifest(
           DictionaryBuilder().Set("permissions", permissions));
}

}  // namespace extension_test_util
}  // namespace extensions

namespace extension_test_util {

scoped_refptr<Extension> CreateExtensionWithID(std::string id) {
  base::DictionaryValue values;
  values.SetString(extension_manifest_keys::kName, "test");
  values.SetString(extension_manifest_keys::kVersion, "0.1");
  std::string error;
  return Extension::Create(base::FilePath(), extensions::Manifest::INTERNAL,
                           values, Extension::NO_FLAGS, id, &error);
}

scoped_refptr<Extension> LoadManifestUnchecked(const std::string& dir,
                                               const std::string& test_file,
                                               Manifest::Location location,
                                               int extra_flags,
                                               const std::string& id,
                                               std::string* error) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueSerializer serializer(path);
  scoped_ptr<base::Value> result(serializer.Deserialize(NULL, error));
  if (!result)
    return NULL;
  const base::DictionaryValue* dict;
  CHECK(result->GetAsDictionary(&dict));

  scoped_refptr<Extension> extension = Extension::Create(
      path.DirName(), location, *dict, extra_flags, id, error);
  return extension;
}

scoped_refptr<Extension> LoadManifestUnchecked(const std::string& dir,
                                               const std::string& test_file,
                                               Manifest::Location location,
                                               int extra_flags,
                                               std::string* error) {
  return LoadManifestUnchecked(
      dir, test_file, location, extra_flags, std::string(), error);
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file,
                                      Manifest::Location location,
                                      int extra_flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadManifestUnchecked(dir, test_file, location, extra_flags, &error);

  EXPECT_TRUE(extension.get()) << test_file << ":" << error;
  return extension;
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file,
                                      int extra_flags) {
  return LoadManifest(dir, test_file, Manifest::INVALID_LOCATION, extra_flags);
}

scoped_refptr<Extension> LoadManifestStrict(const std::string& dir,
                                            const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

}  // namespace extension_test_util
