// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

bool HexStringEquals(std::string hex_string, const std::string* bytes) {
  if (!bytes)
    return false;
  std::vector<uint8> decoded;
  if (!base::HexStringToBytes(hex_string, &decoded))
    return false;
  if (decoded.size() != bytes->size())
    return false;

  if (bytes->empty())
    return true;

  return memcmp(vector_as_array(&decoded), bytes->data(), bytes->size()) == 0;
}

bool GetPublicKey(const base::FilePath& path, std::string* public_key) {
  std::string public_key_pem;
  if (!base::ReadFileToString(path, &public_key_pem))
    return false;
  if (!Extension::ParsePEMKeyBytes(public_key_pem, public_key))
    return false;
  return true;
}

}  // namespace

TEST(VerifiedContents, Simple) {
  // Figure out our test data directory.
  base::FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);
  path = path.AppendASCII("content_verifier/");

  // Initialize the VerifiedContents object.
  std::string public_key;
  ASSERT_TRUE(GetPublicKey(path.AppendASCII("public_key.pem"), &public_key));
  VerifiedContents contents(reinterpret_cast<const uint8*>(public_key.data()),
                            public_key.size());
  base::FilePath verified_contents_path =
      path.AppendASCII("verified_contents.json");

  ASSERT_TRUE(contents.InitFrom(verified_contents_path, false));

  // Make sure we get expected values.
  EXPECT_EQ(contents.block_size(), 4096);
  EXPECT_EQ(contents.extension_id(), "abcdefghijklmnopabcdefghijklmnop");
  EXPECT_EQ("1.2.3", contents.version().GetString());

  EXPECT_TRUE(HexStringEquals(
      "fafcb22089fb8920b383b5f7202508e7065aded1bbc3bbf2882724c5974919fb",
      contents.GetTreeHashRoot(
          base::FilePath::FromUTF8Unsafe("manifest.json"))));
  EXPECT_TRUE(HexStringEquals(
      "b711e21b9290bcda0f3921f915b428f596f9809db78f7a0507446ef433a7ce2c",
      contents.GetTreeHashRoot(
          base::FilePath::FromUTF8Unsafe("background.js"))));
  EXPECT_TRUE(HexStringEquals(
      "2f7ecb15b4ff866b7144bec07c664df584e95baca8cff662435a292c99f53595",
      contents.GetTreeHashRoot(
          base::FilePath::FromUTF8Unsafe("foo/bar.html"))));

  base::FilePath nonexistent = base::FilePath::FromUTF8Unsafe("nonexistent");
  EXPECT_TRUE(contents.GetTreeHashRoot(nonexistent) == NULL);
}

}  // namespace extensions
