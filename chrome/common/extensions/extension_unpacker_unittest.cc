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
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

class ExtensionUnpackerTest : public testing::Test {
public:
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
            crx_path, Extension::INTERNAL, Extension::NO_FLAGS));
  }

 protected:
  ScopedTempDir temp_dir_;
  scoped_ptr<ExtensionUnpacker> unpacker_;
};

// Crashes intermittently on Windows, see http://crbug.com/109238
// TODO(mpcomplete): Temporary enabled to debug.
#if 0  // defined(OS_WIN)
#define MAYBE_EmptyDefaultLocale DISABLED_EmptyDefaultLocale
#else
#define MAYBE_EmptyDefaultLocale EmptyDefaultLocale
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_EmptyDefaultLocale) {
  g_bug108724_debug = true;
  LOG(WARNING) << "Setting up.";
  SetupUnpacker("empty_default_locale.crx");
  LOG(WARNING) << "Running unpacker.";
  EXPECT_FALSE(unpacker_->Run());
  LOG(WARNING) << "Done.";
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
  LOG(WARNING) << "Equal.";
  LOG(WARNING) << "Cleanup: " << temp_dir_.Delete();
  LOG(WARNING) << "Cleanup2:";
  unpacker_.reset();
  LOG(WARNING) << "All clean.";
  g_bug108724_debug = false;
}

// Crashes intermittently on Vista, see http://crbug.com/109385
// TODO(mpcomplete): Temporary enabled to debug.
#if 0  // defined(OS_WIN)
#define MAYBE_HasDefaultLocaleMissingLocalesFolder \
    DISABLED_HasDefaultLocaleMissingLocalesFolder
#else
#define MAYBE_HasDefaultLocaleMissingLocalesFolder \
    HasDefaultLocaleMissingLocalesFolder
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_HasDefaultLocaleMissingLocalesFolder) {
  g_bug108724_debug = true;
  LOG(WARNING) << "Setting up.";
  SetupUnpacker("has_default_missing_locales.crx");
  LOG(WARNING) << "Running unpacker.";
  EXPECT_FALSE(unpacker_->Run());
  LOG(WARNING) << "Done.";
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesTreeMissing),
            unpacker_->error_message());
  LOG(WARNING) << "Equal.";
  LOG(WARNING) << "Cleanup: " << temp_dir_.Delete();
  LOG(WARNING) << "Cleanup2:";
  unpacker_.reset();
  LOG(WARNING) << "All clean.";
  g_bug108724_debug = false;
}

// Crashes intermittently on Windows, see http://crbug.com/109238
// TODO(mpcomplete): Temporary enabled to debug.
#if 0  // defined(OS_WIN)
#define MAYBE_InvalidDefaultLocale DISABLED_InvalidDefaultLocale
#else
#define MAYBE_InvalidDefaultLocale InvalidDefaultLocale
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_InvalidDefaultLocale) {
  g_bug108724_debug = true;
  LOG(WARNING) << "Setting up.";
  SetupUnpacker("invalid_default_locale.crx");
  LOG(WARNING) << "Running unpacker.";
  EXPECT_FALSE(unpacker_->Run());
  LOG(WARNING) << "Done.";
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
  LOG(WARNING) << "Equal.";
  LOG(WARNING) << "Cleanup: " << temp_dir_.Delete();
  LOG(WARNING) << "Cleanup2:";
  unpacker_.reset();
  LOG(WARNING) << "All clean.";
  g_bug108724_debug = false;
}

// Crashes intermittently on Windows, see http://crbug.com/109738
#if defined(OS_WIN)
#define MAYBE_InvalidMessagesFile DISABLE_InvalidMessagesFile
#else
#define MAYBE_InvalidMessagesFile InvalidMessagesFile
#endif
TEST_F(ExtensionUnpackerTest, MAYBE_InvalidMessagesFile) {
  SetupUnpacker("invalid_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    ASCIIToUTF16("*_locales?en_US?messages.json: Line: 2, column: 3,"
                " Dictionary keys must be quoted.")));
}

TEST_F(ExtensionUnpackerTest, MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultLocaleSpecified),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    ASCIIToUTF16(errors::kLocalesMessagesFileMissing) +
    ASCIIToUTF16("*_locales?en_US?messages.json")));
}

TEST_F(ExtensionUnpackerTest, NoLocaleData) {
  SetupUnpacker("no_locale_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, GoodL10n) {
  SetupUnpacker("good_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  ASSERT_EQ(2U, unpacker_->parsed_catalogs()->size());
}

TEST_F(ExtensionUnpackerTest, NoL10n) {
  SetupUnpacker("no_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  EXPECT_EQ(0U, unpacker_->parsed_catalogs()->size());
}
