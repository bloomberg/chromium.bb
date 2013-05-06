// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kValidImportPath =
    "_modules/abcdefghijklmnopabcdefghijklmnop/foo/bar.html";
const char* kValidImportPathID = "abcdefghijklmnopabcdefghijklmnop";
const char* kValidImportPathRelative = "foo/bar.html";
const char* kInvalidImportPath = "_modules/abc/foo.html";
const char* kImportId1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char* kImportId2 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char* kNoImport = "cccccccccccccccccccccccccccccccc";

}

namespace extensions {

class SharedModuleManifestTest : public ExtensionManifestTest {
};

TEST_F(SharedModuleManifestTest, ExportsAll) {
  Manifest manifest("shared_module_export.json");

  scoped_refptr<Extension> extension = LoadAndExpectSuccess(manifest);

  EXPECT_TRUE(SharedModuleInfo::IsSharedModule(extension)) << manifest.name();
  EXPECT_FALSE(SharedModuleInfo::ImportsModules(extension)) << manifest.name();
  EXPECT_TRUE(SharedModuleInfo::IsExportAllowed(extension, "foo"))
      << manifest.name();
  EXPECT_TRUE(SharedModuleInfo::IsExportAllowed(extension, "foo/bar"))
      << manifest.name();
}

TEST_F(SharedModuleManifestTest, ExportFoo) {
  Manifest manifest("shared_module_export_foo.json");

  scoped_refptr<Extension> extension = LoadAndExpectSuccess(manifest);

  EXPECT_TRUE(SharedModuleInfo::IsSharedModule(extension)) << manifest.name();
  EXPECT_FALSE(SharedModuleInfo::ImportsModules(extension)) << manifest.name();
  EXPECT_TRUE(SharedModuleInfo::IsExportAllowed(extension, "foo"))
      << manifest.name();
  EXPECT_FALSE(SharedModuleInfo::IsExportAllowed(extension, "foo/bar"))
      << manifest.name();
}

TEST_F(SharedModuleManifestTest, ExportParseErrors) {
  Testcase testcases[] = {
    Testcase("shared_module_export_and_import.json",
             "Simultaneous 'import' and 'export' are not allowed."),
    Testcase("shared_module_export_not_dict.json",
             "Invalid value for 'export'."),
    Testcase("shared_module_export_resources_not_list.json",
             "Invalid value for 'export.resources'."),
    Testcase("shared_module_export_resource_not_string.json",
             "Invalid value for 'export.resources[1]'."),
  };
  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(SharedModuleManifestTest, SharedModuleStaticFunctions) {
  EXPECT_TRUE(SharedModuleInfo::IsImportedPath(kValidImportPath));
  EXPECT_FALSE(SharedModuleInfo::IsImportedPath(kInvalidImportPath));

  std::string id;
  std::string relative;
  SharedModuleInfo::ParseImportedPath(kValidImportPath, &id, &relative);
  EXPECT_EQ(id, kValidImportPathID);
  EXPECT_EQ(relative, kValidImportPathRelative);
}

TEST_F(SharedModuleManifestTest, Import) {
  Manifest manifest("shared_module_import.json");

  scoped_refptr<Extension> extension = LoadAndExpectSuccess(manifest);

  EXPECT_FALSE(SharedModuleInfo::IsSharedModule(extension)) << manifest.name();
  EXPECT_TRUE(SharedModuleInfo::ImportsModules(extension)) << manifest.name();
  const std::vector<SharedModuleInfo::ImportInfo>& imports =
      SharedModuleInfo::GetImports(extension);
  ASSERT_EQ(2U, imports.size());
  EXPECT_EQ(imports[0].extension_id, kImportId1);
  EXPECT_EQ(imports[0].minimum_version, "");
  EXPECT_EQ(imports[1].extension_id, kImportId2);
  EXPECT_TRUE(base::Version(imports[1].minimum_version).IsValid());
  EXPECT_TRUE(SharedModuleInfo::ImportsExtensionById(extension, kImportId1));
  EXPECT_TRUE(SharedModuleInfo::ImportsExtensionById(extension, kImportId2));
  EXPECT_FALSE(SharedModuleInfo::ImportsExtensionById(extension, kNoImport));
}

TEST_F(SharedModuleManifestTest, ImportParseErrors) {
  Testcase testcases[] = {
    Testcase("shared_module_import_not_list.json",
             "Invalid value for 'import'."),
    Testcase("shared_module_import_invalid_id.json",
             "Invalid value for 'import[0].id'."),
    Testcase("shared_module_import_invalid_version.json",
             "Invalid value for 'import[0].minimum_version'."),
  };
  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}

}  // namespace extensions
