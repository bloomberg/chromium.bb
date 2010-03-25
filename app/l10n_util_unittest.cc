// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <cstdlib>
#endif

#include "app/app_paths.h"
#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#if !defined(OS_MACOSX)
#include "app/test/data/resource.h"
#endif
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "unicode/locid.h"

namespace {

class StringWrapper {
 public:
  explicit StringWrapper(const std::wstring& string) : string_(string) {}
  const std::wstring& string() const { return string_; }

 private:
  std::wstring string_;

  DISALLOW_COPY_AND_ASSIGN(StringWrapper);
};

}  // namespace

class L10nUtilTest : public PlatformTest {
};

#if defined(OS_WIN)
// TODO(beng): disabled until app strings move to app.
TEST_F(L10nUtilTest, DISABLED_GetString) {
  std::wstring s = l10n_util::GetString(IDS_SIMPLE);
  EXPECT_EQ(std::wstring(L"Hello World!"), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS, L"chrome", L"10");
  EXPECT_EQ(std::wstring(L"Hello, chrome. Your number is 10."), s);

  s = l10n_util::GetStringF(IDS_PLACEHOLDERS_2, 20);
  EXPECT_EQ(std::wstring(L"You owe me $20."), s);
}
#endif  // defined(OS_WIN)

TEST_F(L10nUtilTest, TruncateString) {
  std::wstring string(L"foooooey    bxxxar baz");

  // Make sure it doesn't modify the string if length > string length.
  EXPECT_EQ(string, l10n_util::TruncateString(string, 100));

  // Test no characters.
  EXPECT_EQ(L"", l10n_util::TruncateString(string, 0));

  // Test 1 character.
  EXPECT_EQ(L"\x2026", l10n_util::TruncateString(string, 1));

  // Test adds ... at right spot when there is enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026", l10n_util::TruncateString(string, 14));

  // Test adds ... at right spot when there is not enough space in first word.
  EXPECT_EQ(L"f\x2026", l10n_util::TruncateString(string, 2));

  // Test adds ... at right spot when there is not enough room to break at a
  // word boundary.
  EXPECT_EQ(L"foooooey\x2026", l10n_util::TruncateString(string, 11));

  // Test completely truncates string if break is on initial whitespace.
  EXPECT_EQ(L"\x2026", l10n_util::TruncateString(L"   ", 2));
}

void SetICUDefaultLocale(const std::string& locale_string) {
  icu::Locale locale(locale_string.c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
  EXPECT_TRUE(U_SUCCESS(error_code));
}

#if !defined(OS_MACOSX)
// We are disabling this test on MacOS because GetApplicationLocale() as an
// API isn't something that we'll easily be able to unit test in this manner.
// The meaning of that API, on the Mac, is "the locale used by Cocoa's main
// nib file", which clearly can't be stubbed by a test app that doesn't use
// Cocoa.
TEST_F(L10nUtilTest, GetAppLocale) {
  // Use a temporary locale dir so we don't have to actually build the locale
  // dlls for this test.
  FilePath orig_locale_dir;
  PathService::Get(app::DIR_LOCALES, &orig_locale_dir);
  FilePath new_locale_dir;
  EXPECT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("l10n_util_test"),
      &new_locale_dir));
  PathService::Override(app::DIR_LOCALES, new_locale_dir);
  // Make fake locale files.
  std::string filenames[] = {
    "en-US",
    "en-GB",
    "fr",
    "es-419",
    "es",
    "zh-TW",
    "zh-CN",
    "he",
    "fil",
    "nb",
    "am",
  };

#if defined(OS_WIN)
  static const char kLocaleFileExtension[] = ".dll";
#elif defined(OS_POSIX)
  static const char kLocaleFileExtension[] = ".pak";
#endif
  for (size_t i = 0; i < arraysize(filenames); ++i) {
    FilePath filename = new_locale_dir.AppendASCII(
        filenames[i] + kLocaleFileExtension);
    file_util::WriteFile(filename, "", 0);
  }

  // Keep a copy of ICU's default locale before we overwrite it.
  icu::Locale locale = icu::Locale::getDefault();

#if defined(OS_POSIX) && !defined(OS_CHROMEOS)
  // Test the support of LANGUAGE environment variable.
  SetICUDefaultLocale("en-US");
  ::setenv("LANGUAGE", "xx:fr_CA", 1);
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L""));

  ::setenv("LANGUAGE", "xx:yy:en_gb.utf-8@quot", 1);
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L""));

  ::setenv("LANGUAGE", "xx:zh-hk", 1);
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(L""));

  // We emulate gettext's behavior here, which ignores LANG/LC_MESSAGES/LC_ALL
  // when LANGUAGE is specified. If no language specified in LANGUAGE is valid,
  // then just fallback to the default language, which is en-US for us.
  SetICUDefaultLocale("fr-FR");
  ::setenv("LANGUAGE", "xx:yy", 1);
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

  ::setenv("LANGUAGE", "/fr:zh_CN", 1);
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(L""));

  // Make sure the follow tests won't be affected by LANGUAGE environment
  // variable.
  ::unsetenv("LANGUAGE");
#endif  // defined(OS_POSIX) && !defined(OS_CHROMEOS)

  SetICUDefaultLocale("en-US");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("xx");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

#if defined(OS_CHROMEOS)
  // ChromeOS honors preferred locale first in GetApplicationLocale(),
  // defaulting to en-US, while other targets first honor other signals.
  SetICUDefaultLocale("en-GB");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("en-US");
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L"en-GB"));

#else  // defined(OS_CHROMEOS)
  SetICUDefaultLocale("en-GB");
  EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("fr-CA");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es-MX");
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es-AR");
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es-ES");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("es");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("zh-HK");
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("zh-MK");
  EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(L""));

  SetICUDefaultLocale("zh-SG");
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(L""));
#endif  // defined (OS_CHROMEOS)

#if defined(OS_WIN)
  // We don't allow user prefs for locale on linux/mac.
  SetICUDefaultLocale("en-US");
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L"fr"));
  EXPECT_EQ("fr", l10n_util::GetApplicationLocale(L"fr-CA"));

  SetICUDefaultLocale("en-US");
  // Aliases iw, no, tl to he, nb, fil.
  EXPECT_EQ("he", l10n_util::GetApplicationLocale(L"iw"));
  EXPECT_EQ("nb", l10n_util::GetApplicationLocale(L"no"));
  EXPECT_EQ("fil", l10n_util::GetApplicationLocale(L"tl"));
  // es-419 and es-XX (where XX is not Spain) should be
  // mapped to es-419 (Latin American Spanish).
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L"es-419"));
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L"es-ES"));
  EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(L"es-AR"));

  SetICUDefaultLocale("es-AR");
  EXPECT_EQ("es", l10n_util::GetApplicationLocale(L"es"));

  SetICUDefaultLocale("zh-HK");
  EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(L"zh-CN"));

  SetICUDefaultLocale("he");
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L"en"));

  // Amharic should be blocked unless OS is Vista or newer.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    SetICUDefaultLocale("am");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(L""));
    SetICUDefaultLocale("en-GB");
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(L"am"));
  } else {
    SetICUDefaultLocale("am");
    EXPECT_EQ("am", l10n_util::GetApplicationLocale(L""));
    SetICUDefaultLocale("en-GB");
    EXPECT_EQ("am", l10n_util::GetApplicationLocale(L"am"));
  }
#endif  // defined(OS_WIN)

  // Clean up.
  PathService::Override(app::DIR_LOCALES, orig_locale_dir);
  file_util::Delete(new_locale_dir, true);
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
}
#endif  // !defined(OS_MACOSX)

TEST_F(L10nUtilTest, SortStringsUsingFunction) {
  std::vector<StringWrapper*> strings;
  strings.push_back(new StringWrapper(L"C"));
  strings.push_back(new StringWrapper(L"d"));
  strings.push_back(new StringWrapper(L"b"));
  strings.push_back(new StringWrapper(L"a"));
  l10n_util::SortStringsUsingMethod(L"en-US", &strings, &StringWrapper::string);
  ASSERT_TRUE(L"a" == strings[0]->string());
  ASSERT_TRUE(L"b" == strings[1]->string());
  ASSERT_TRUE(L"C" == strings[2]->string());
  ASSERT_TRUE(L"d" == strings[3]->string());
  STLDeleteElements(&strings);
}

// Test upper and lower case string conversion.
TEST_F(L10nUtilTest, UpperLower) {
  string16 mixed(ASCIIToUTF16("Text with UPPer & lowER casE."));
  const string16 expected_lower(ASCIIToUTF16("text with upper & lower case."));
  const string16 expected_upper(ASCIIToUTF16("TEXT WITH UPPER & LOWER CASE."));

  string16 result = l10n_util::ToLower(mixed);
  EXPECT_EQ(result, expected_lower);

  result = l10n_util::ToUpper(mixed);
  EXPECT_EQ(result, expected_upper);
}

TEST_F(L10nUtilTest, LocaleDisplayName) {
  // TODO(jungshik): Make this test more extensive.
  // Test zh-CN and zh-TW are treated as zh-Hans and zh-Hant.
  string16 result = l10n_util::GetDisplayNameForLocale("zh-CN", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Chinese (Simplified Han)"));

  result = l10n_util::GetDisplayNameForLocale("zh-TW", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Chinese (Traditional Han)"));

  result = l10n_util::GetDisplayNameForLocale("pt-BR", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Portuguese (Brazil)"));

  result = l10n_util::GetDisplayNameForLocale("es-419", "en", false);
  EXPECT_EQ(result, ASCIIToUTF16("Spanish (Latin America and the Caribbean)"));
}
