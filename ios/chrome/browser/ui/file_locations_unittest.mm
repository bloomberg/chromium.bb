// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/file_locations.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

extern std::string GetLocalizedFileName(const std::string& base_name,
                                        const std::string& locale,
                                        const std::string& ext);

extern std::string GetIOSLocaleMapping(const std::string& locale);

namespace {

class FileLocationsTest : public PlatformTest {
 protected:
  void SetUp() override {
    // These files must exist for this unit test to pass.
    termsBaseName_ = "terms";
    extension_ = "html";
    enFile_ = "terms_en.html";
    frFile_ = "terms_fr.html";
  }
  std::string termsBaseName_;
  std::string extension_;
  std::string enFile_;
  std::string frFile_;
};

TEST_F(FileLocationsTest, TestTermsOfServiceUrl) {
  std::string filename(GetTermsOfServicePath());
  EXPECT_FALSE(filename.empty());
}

TEST_F(FileLocationsTest, TestIOSLocaleMapping) {
  EXPECT_EQ("en-US", GetIOSLocaleMapping("en-US"));
  EXPECT_EQ("es", GetIOSLocaleMapping("es"));
  EXPECT_EQ("es-419", GetIOSLocaleMapping("es-MX"));
  EXPECT_EQ("pt-BR", GetIOSLocaleMapping("pt"));
  EXPECT_EQ("pt-PT", GetIOSLocaleMapping("pt-PT"));
  EXPECT_EQ("zh-CN", GetIOSLocaleMapping("zh-Hans"));
  EXPECT_EQ("zh-TW", GetIOSLocaleMapping("zh-Hant"));
}

TEST_F(FileLocationsTest, TestFileNameLocaleWithExtension) {
  EXPECT_EQ(enFile_, GetLocalizedFileName(termsBaseName_, "en", extension_));
  EXPECT_EQ(frFile_, GetLocalizedFileName(termsBaseName_, "fr", extension_));
  EXPECT_EQ(frFile_, GetLocalizedFileName(termsBaseName_, "fr-XX", extension_));

  // No ToS for "xx" locale so expect default "en" ToS. Unlikely, but this
  // test will fail once the ToS for "xx" locale is added.
  EXPECT_EQ(enFile_, GetLocalizedFileName(termsBaseName_, "xx", extension_));
}

// Tests that locale/languages available on iOS are mapped to either a
// translated Chrome Terms of Service or to English.
TEST_F(FileLocationsTest, TestTermsOfServiceForSupportedLanguages) {
  // List of available localized terms_*.html files. Note that this list is
  // hardcoded and needs to be manually maintained as new locales are added
  // to Chrome. See http://crbug/522638
  NSSet* localizedTermsHtml =
      [NSSet setWithObjects:@"ar", @"bg", @"ca", @"cs", @"da", @"de", @"el",
                            @"en-GB", @"en", @"es-419", @"es", @"fa", @"fi",
                            @"fr", @"he", @"hi", @"hr", @"hu", @"id", @"it",
                            @"ja", @"ko", @"lt", @"nb", @"nl", @"pl", @"pt-BR",
                            @"pt-PT", @"ro", @"ru", @"sk", @"sr", @"sv", @"th",
                            @"tr", @"uk", @"vi", @"zh-CN", @"zh-TW", nil];
  for (NSString* locale in [NSLocale availableLocaleIdentifiers]) {
    NSString* normalizedLocale =
        [locale stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
    NSArray* parts = [normalizedLocale componentsSeparatedByString:@"-"];
    NSString* language = [parts objectAtIndex:0];
    std::string filename = GetLocalizedFileName(
        termsBaseName_,
        GetIOSLocaleMapping(base::SysNSStringToUTF8(normalizedLocale)),
        extension_);
    if (filename == enFile_ && ![language isEqualToString:@"en"]) {
      NSLog(@"OK: Locale %@ using language %@ falls back to use English ToS",
            locale, language);
      EXPECT_FALSE([localizedTermsHtml containsObject:language]);
    }
  }
}

}  // namespace
