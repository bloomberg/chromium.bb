// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void GetJSonData(const std::string& full_name,
                 const std::string& given_name,
                 const std::string& url,
                 const std::string& locale,
                 const std::string& hosted_domain,
                 bool include_empty_hosted_domain,
                 base::DictionaryValue* dict) {
  if (!full_name.empty())
    dict->SetString("name", full_name);

  if (!given_name.empty())
    dict->SetString("given_name", given_name);

  if (!url.empty())
    dict->SetString("picture", url);

  if (!locale.empty())
    dict->SetString("locale", locale);

  if (!hosted_domain.empty() || include_empty_hosted_domain)
    dict->SetString("hd", hosted_domain);
}

} // namespace

class ProfileDownloaderTest : public testing::Test {
 protected:
  ProfileDownloaderTest() {
  }

  ~ProfileDownloaderTest() override {}

  void VerifyWithAccountData(const std::string& full_name,
                             const std::string& given_name,
                             const std::string& url,
                             const std::string& expected_url,
                             const std::string& locale,
                             const std::string& hosted_domain,
                             bool include_empty_hosted_domain,
                             bool is_valid) {
    base::string16 parsed_full_name;
    base::string16 parsed_given_name;
    std::string parsed_url;
    std::string parsed_locale;
    base::string16 parsed_hosted_domain;
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    GetJSonData(full_name, given_name, url, locale, hosted_domain,
                include_empty_hosted_domain, dict.get());
    bool result = ProfileDownloader::ParseProfileJSON(
        dict.get(),
        &parsed_full_name,
        &parsed_given_name,
        &parsed_url,
        32,
        &parsed_locale,
        &parsed_hosted_domain);
    EXPECT_EQ(is_valid, result);
    std::string parsed_full_name_utf8 = base::UTF16ToUTF8(parsed_full_name);
    std::string parsed_given_name_utf8 = base::UTF16ToUTF8(parsed_given_name);
    std::string parsed_hosted_domain_utf8 =
        base::UTF16ToUTF8(parsed_hosted_domain);

    EXPECT_EQ(full_name, parsed_full_name_utf8);
    EXPECT_EQ(given_name, parsed_given_name_utf8);
    EXPECT_EQ(expected_url, parsed_url);
    EXPECT_EQ(locale, parsed_locale);
    EXPECT_EQ(hosted_domain, parsed_hosted_domain_utf8);
  }
};

TEST_F(ProfileDownloaderTest, ParseData) {
  // URL without size specified.
  VerifyWithAccountData(
      "Pat Smith",
      "Pat",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s32-c/photo.jpg",
      "en-US",
      "google.com",
      false,
      true);

  // URL with size specified.
  VerifyWithAccountData(
      "Pat Smith",
      "Pat",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s64-c/1234567890.jpg",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s32-c/1234567890.jpg",
      "en-US",
      "google.com",
      false,
      true);

  // URL with unknown format.
  VerifyWithAccountData("Pat Smith",
                        "Pat",
                        "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/",
                        "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/",
                        "en-US",
                        "google.com",
                        false,
                        true);

  // Try different locales. URL with size specified.
  VerifyWithAccountData(
      "Pat Smith",
      "Pat",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s64-c/1234567890.jpg",
      "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s32-c/1234567890.jpg",
      "jp",
      "google.com",
      false,
      true);

  // URL with unknown format.
  VerifyWithAccountData("Pat Smith",
                        "Pat",
                        "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/",
                        "http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/",
                        "fr",
                        "",
                        false,
                        true);

  // Data with only name.
  VerifyWithAccountData("Pat Smith",
                        "Pat",
                        std::string(),
                        std::string(),
                        std::string(),
                        std::string(),
                        false,
                        true);

  // Data with only name and a blank but present hosted domain.
  VerifyWithAccountData("Pat Smith",
                        "Pat",
                        std::string(),
                        std::string(),
                        std::string(),
                        std::string(),
                        true,
                        true);

  // Data with only URL.
  VerifyWithAccountData(
      std::string(),
      std::string(),
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg",
      "https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s32-c/photo.jpg",
      std::string(),
      std::string(),
      false,
      true);

  // Data with only locale.
  VerifyWithAccountData(std::string(),
                        std::string(),
                        std::string(),
                        std::string(),
                        "fr",
                        std::string(),
                        false,
                        false);

  // Data without name or URL or locale.
  VerifyWithAccountData(std::string(),
                        std::string(),
                        std::string(),
                        std::string(),
                        std::string(),
                        std::string(),
                        false,
                        false);

  // Data with an invalid URL.
  VerifyWithAccountData(std::string(),
                        std::string(),
                        "invalid url",
                        std::string(),
                        std::string(),
                        std::string(),
                        false,
                        false);
}

TEST_F(ProfileDownloaderTest, DefaultURL) {
  // Empty URL should be default photo
  EXPECT_TRUE(ProfileDownloader::IsDefaultProfileImageURL(std::string()));
  // Picasa default photo
  EXPECT_TRUE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAA/AAAAAAAAAAE/G/s64-c/photo.jpg"));
  // Not default G+ photo
  EXPECT_FALSE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAI/AAAAAAAAAAA/G/photo.jpg"));
  // Not default with 6 components
  EXPECT_FALSE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg"));
  // Not default with 7 components
  EXPECT_FALSE(ProfileDownloader::IsDefaultProfileImageURL(
      "https://example.com/-4/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s32-c/photo.jpg"));
}
