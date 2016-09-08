// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_dir.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TestExtensionDir::TestExtensionDir() {
  EXPECT_TRUE(dir_.CreateUniqueTempDir());
  EXPECT_TRUE(crx_dir_.CreateUniqueTempDir());
}

TestExtensionDir::~TestExtensionDir() {
}

void TestExtensionDir::WriteManifest(base::StringPiece manifest) {
  WriteFile(FILE_PATH_LITERAL("manifest.json"), manifest);
}

void TestExtensionDir::WriteManifestWithSingleQuotes(
    base::StringPiece manifest) {
  std::string double_quotes;
  base::ReplaceChars(manifest.data(), "'", "\"", &double_quotes);
  WriteManifest(double_quotes);
}

void TestExtensionDir::WriteFile(const base::FilePath::StringType& filename,
                                 base::StringPiece contents) {
  EXPECT_EQ(base::checked_cast<int>(contents.size()),
            base::WriteFile(dir_.GetPath().Append(filename), contents.data(),
                            contents.size()));
}

// This function packs the extension into a .crx, and returns the path to that
// .crx. Multiple calls to Pack() will produce extensions with the same ID.
base::FilePath TestExtensionDir::Pack() {
  ExtensionCreator creator;
  base::FilePath crx_path =
      crx_dir_.GetPath().Append(FILE_PATH_LITERAL("ext.crx"));
  base::FilePath pem_path =
      crx_dir_.GetPath().Append(FILE_PATH_LITERAL("ext.pem"));
  base::FilePath pem_in_path, pem_out_path;
  if (base::PathExists(pem_path))
    pem_in_path = pem_path;
  else
    pem_out_path = pem_path;
  if (!creator.Run(dir_.GetPath(), crx_path, pem_in_path, pem_out_path,
                   ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE()
        << "ExtensionCreator::Run() failed: " << creator.error_message();
    return base::FilePath();
  }
  if (!base::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return base::FilePath();
  }
  return crx_path;
}

base::FilePath TestExtensionDir::UnpackedPath() {
  return dir_.GetPath();
}

}  // namespace extensions
