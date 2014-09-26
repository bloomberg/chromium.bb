// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

std::string DecodeBase64Url(const std::string& encoded) {
  std::string fixed_up_base64 = encoded;
  if (!VerifiedContents::FixupBase64Encoding(&fixed_up_base64))
    return std::string();
  std::string decoded;
  if (!base::Base64Decode(fixed_up_base64, &decoded))
    return std::string();
  return decoded;
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

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("manifest.json"),
      DecodeBase64Url("-vyyIIn7iSCzg7X3ICUI5wZa3tG7w7vyiCckxZdJGfs")));

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("background.js"),
      DecodeBase64Url("txHiG5KQvNoPOSH5FbQo9Zb5gJ23j3oFB0Ru9DOnziw")));

  base::FilePath foo_bar_html =
      base::FilePath(FILE_PATH_LITERAL("foo")).AppendASCII("bar.html");
  EXPECT_FALSE(foo_bar_html.IsAbsolute());
  EXPECT_TRUE(contents.TreeHashRootEquals(
      foo_bar_html,
      DecodeBase64Url("L37LFbT_hmtxRL7AfGZN9YTpW6yoz_ZiQ1opLJn1NZU")));

  base::FilePath nonexistent = base::FilePath::FromUTF8Unsafe("nonexistent");
  EXPECT_FALSE(contents.HasTreeHashRoot(nonexistent));

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("lowercase.html"),
      DecodeBase64Url("HpLotLGCmmOdKYvGQmD3OkXMKGs458dbanY4WcfAZI0")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("Lowercase.Html"),
      DecodeBase64Url("HpLotLGCmmOdKYvGQmD3OkXMKGs458dbanY4WcfAZI0")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("LOWERCASE.HTML"),
      DecodeBase64Url("HpLotLGCmmOdKYvGQmD3OkXMKGs458dbanY4WcfAZI0")));

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("ALLCAPS.HTML"),
      DecodeBase64Url("bl-eV8ENowvtw6P14D4X1EP0mlcMoG-_aOx5o9C1364")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("AllCaps.Html"),
      DecodeBase64Url("bl-eV8ENowvtw6P14D4X1EP0mlcMoG-_aOx5o9C1364")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("allcaps.html"),
      DecodeBase64Url("bl-eV8ENowvtw6P14D4X1EP0mlcMoG-_aOx5o9C1364")));

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("MixedCase.Html"),
      DecodeBase64Url("zEAO9FwciigMNy3NtU2XNb-dS5TQMmVNx0T9h7WvXbQ")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("MIXEDCASE.HTML"),
      DecodeBase64Url("zEAO9FwciigMNy3NtU2XNb-dS5TQMmVNx0T9h7WvXbQ")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("mixedcase.html"),
      DecodeBase64Url("zEAO9FwciigMNy3NtU2XNb-dS5TQMmVNx0T9h7WvXbQ")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("mIxedcAse.Html"),
      DecodeBase64Url("zEAO9FwciigMNy3NtU2XNb-dS5TQMmVNx0T9h7WvXbQ")));

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("mIxedcAse.Html"),
      DecodeBase64Url("nKRqUcJg1_QZWAeCb4uFd5ouC0McuGavKp8TFDRqBgg")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("MIXEDCASE.HTML"),
      DecodeBase64Url("nKRqUcJg1_QZWAeCb4uFd5ouC0McuGavKp8TFDRqBgg")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("mixedcase.html"),
      DecodeBase64Url("nKRqUcJg1_QZWAeCb4uFd5ouC0McuGavKp8TFDRqBgg")));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("MixedCase.Html"),
      DecodeBase64Url("nKRqUcJg1_QZWAeCb4uFd5ouC0McuGavKp8TFDRqBgg")));
}

}  // namespace extensions
