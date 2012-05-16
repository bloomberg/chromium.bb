// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

class ExtensionUnpackerTest : public testing::Test {
public:
  ~ExtensionUnpackerTest() {
    LOG(WARNING) << "Deleting temp dir: "
                 << temp_dir_.path().LossyDisplayName();
    LOG(WARNING) << temp_dir_.Delete();
  }

  void SetupUnpacker(const std::string& crx_name) {
    FilePath original_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &original_path));
    original_path = original_path.AppendASCII("extensions")
        .AppendASCII("unpacker")
        .AppendASCII(crx_name);
    ASSERT_TRUE(file_util::PathExists(original_path)) << original_path.value();

    // Try bots won't let us write into DIR_TEST_DATA, so we have to create
    // a temp folder to play in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    FilePath crx_path = temp_dir_.path().AppendASCII(crx_name);
    ASSERT_TRUE(file_util::CopyFile(original_path, crx_path)) <<
        "Original path " << original_path.value() <<
        ", Crx path " << crx_path.value();

    unpacker_.reset(
        new ExtensionUnpacker(
            crx_path, std::string(), Extension::INTERNAL, Extension::NO_FLAGS));
  }

 protected:
  ScopedTempDir temp_dir_;
  scoped_ptr<ExtensionUnpacker> unpacker_;
};

// Crashes intermittently on Windows, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_EmptyDefaultLocale DISABLED_EmptyDefaultLocale
#else
#define MAYBE_EmptyDefaultLocale EmptyDefaultLocale
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_EmptyDefaultLocale) {
  SetupUnpacker("empty_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

// Crashes intermittently on Vista, see http://crbug.com/109385
#if defined(OS_WIN)
#define MAYBE_HasDefaultLocaleMissingLocalesFolder \
  DISABLED_HasDefaultLocaleMissingLocalesFolder
#else
#define MAYBE_HasDefaultLocaleMissingLocalesFolder \
  HasDefaultLocaleMissingLocalesFolder
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_HasDefaultLocaleMissingLocalesFolder) {
  SetupUnpacker("has_default_missing_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesTreeMissing),
            unpacker_->error_message());
}

// Crashes intermittently on Windows, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_InvalidDefaultLocale DISABLED_InvalidDefaultLocale
#else
#define MAYBE_InvalidDefaultLocale InvalidDefaultLocale
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_InvalidDefaultLocale) {
  SetupUnpacker("invalid_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

// Crashes intermittently on Windows, see http://crbug.com/109738
#if defined(OS_WIN)
#define MAYBE_InvalidMessagesFile DISABLED_InvalidMessagesFile
#else
#define MAYBE_InvalidMessagesFile InvalidMessagesFile
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_InvalidMessagesFile) {
  SetupUnpacker("invalid_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    ASCIIToUTF16("*_locales?en_US?messages.json: Line: 2, column: 11,"
        " Syntax error."))) << unpacker_->error_message();
}

// Crashes intermittently on Vista, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_MissingDefaultData DISABLED_MissingDefaultData
#else
#define MAYBE_MissingDefaultData MissingDefaultData
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

// Crashes intermittently on Vista, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_MissingDefaultLocaleHasLocalesFolder \
  DISABLED_MissingDefaultLocaleHasLocalesFolder
#else
#define MAYBE_MissingDefaultLocaleHasLocalesFolder \
  MissingDefaultLocaleHasLocalesFolder
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultLocaleSpecified),
            unpacker_->error_message());
}

// Crashes intermittently on Vista, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_MissingMessagesFile DISABLED_MissingMessagesFile
#else
#define MAYBE_MissingMessagesFile MissingMessagesFile
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    ASCIIToUTF16(errors::kLocalesMessagesFileMissing) +
    ASCIIToUTF16("*_locales?en_US?messages.json")));
}

// Crashes intermittently on Vista, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_NoLocaleData DISABLED_NoLocaleData
#else
#define MAYBE_NoLocaleData NoLocaleData
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_NoLocaleData) {
  SetupUnpacker("no_locale_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

// Crashes intermittently on Vista, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_GoodL10n DISABLED_GoodL10n
#else
#define MAYBE_GoodL10n GoodL10n
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_GoodL10n) {
  SetupUnpacker("good_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  ASSERT_EQ(2U, unpacker_->parsed_catalogs()->size());
}

// Crashes intermittently on Vista, see http://crbug.com/109238
#if defined(OS_WIN)
#define MAYBE_NoL10n DISABLED_NoL10n
#else
#define MAYBE_NoL10n NoL10n
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_NoL10n) {
  SetupUnpacker("no_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  EXPECT_EQ(0U, unpacker_->parsed_catalogs()->size());
}
