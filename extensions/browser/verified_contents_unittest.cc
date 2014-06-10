// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

bool Base64UrlStringEquals(std::string input, const std::string* bytes) {
  if (!bytes)
    return false;
  if (!VerifiedContents::FixupBase64Encoding(&input))
    return false;
  std::string decoded;
  if (!base::Base64Decode(input, &decoded))
    return false;
  if (decoded.size() != bytes->size())
    return false;

  if (bytes->empty())
    return true;

  return decoded == *bytes;
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

  EXPECT_TRUE(Base64UrlStringEquals(
      "-vyyIIn7iSCzg7X3ICUI5wZa3tG7w7vyiCckxZdJGfs",
      contents.GetTreeHashRoot(
          base::FilePath::FromUTF8Unsafe("manifest.json"))));
  EXPECT_TRUE(Base64UrlStringEquals(
      "txHiG5KQvNoPOSH5FbQo9Zb5gJ23j3oFB0Ru9DOnziw",
      contents.GetTreeHashRoot(
          base::FilePath::FromUTF8Unsafe("background.js"))));

  base::FilePath foo_bar_html =
      base::FilePath(FILE_PATH_LITERAL("foo")).AppendASCII("bar.html");
  EXPECT_FALSE(foo_bar_html.IsAbsolute());
  EXPECT_TRUE(
      Base64UrlStringEquals("L37LFbT_hmtxRL7AfGZN9YTpW6yoz_ZiQ1opLJn1NZU",
                            contents.GetTreeHashRoot(foo_bar_html)));

  base::FilePath nonexistent = base::FilePath::FromUTF8Unsafe("nonexistent");
  EXPECT_TRUE(contents.GetTreeHashRoot(nonexistent) == NULL);
}

}  // namespace extensions
