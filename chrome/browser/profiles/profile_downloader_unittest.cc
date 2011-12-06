// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetJSonData(const std::string& name, const std::string& url) {
  std::stringstream stream;
  stream << "{ ";
  if (!name.empty())
    stream << "\"name\": \"" << name << "\", ";
  if (!url.empty())
    stream << "\"picture\": \"" << url << "\", ";
  stream << "\"locale\": \"en-US\" }";
  return stream.str();
}

} // namespace

class ProfileDownloaderTest : public testing::Test {
 protected:
  ProfileDownloaderTest() {
  }

  virtual ~ProfileDownloaderTest() {
  }

  void VerifyWithNameAndURL(const std::string& name,
                                   const std::string& url,
                                   const std::string& expected_url,
                                   bool is_valid) {
    string16 parsed_name;
    std::string parsed_url;
    bool result = ProfileDownloader::GetProfileNameAndImageURL(
        GetJSonData(name, url), &parsed_name, &parsed_url, 32);
    EXPECT_EQ(is_valid, result);
    std::string parsed_name_utf8 = UTF16ToUTF8(parsed_name);
    EXPECT_EQ(name, parsed_name_utf8);
    EXPECT_EQ(expected_url, parsed_url);
  }
};

TEST_F(ProfileDownloaderTest, ParseData) {
  // URL without size specified.
  VerifyWithNameAndURL(
      "Pat Smith",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s32-c/photo.jpg",
      true);

  // URL with size specified.
  VerifyWithNameAndURL(
      "Pat Smith",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s64-c/1234567890.jpg",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s32-c/1234567890.jpg",
      true);

  // URL with unknown format.
  VerifyWithNameAndURL(
      "Pat Smith",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/",
      true);

  // Data with only name.
  VerifyWithNameAndURL("Pat Smith", "", "", true);

  // Data with only URL.
  VerifyWithNameAndURL(
      "",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s32-c/photo.jpg",
      true);

  // Data without name or URL.
  VerifyWithNameAndURL("", "", "", false);

  // Data with an invalid URL.
  VerifyWithNameAndURL( "", "invalid url", "", false);
}

TEST_F(ProfileDownloaderTest, DefaultURL) {
  // Empty URL should be default photo
  EXPECT_TRUE(ProfileDownloader::IsDefaultProfileImageURL(""));
  // Picasa default photo
  EXPECT_TRUE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAA/AAAAAAAAAAE/G/s64-c/photo.jpg"));
  // G+ default photo
  EXPECT_TRUE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAI/AAAAAAAAAAA/G/photo.jpg"));
  // Not default with 6 components
  EXPECT_FALSE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg"));
  // Not default with 7 components
  EXPECT_FALSE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s32-c/photo.jpg"));
}
