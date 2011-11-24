// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
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

TEST_F(ExtensionUnpackerTest, EmptyDefaultLocale) {
  SetupUnpacker("empty_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(std::string(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, HasDefaultLocaleMissingLocalesFolder) {
  SetupUnpacker("has_default_missing_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(std::string(errors::kLocalesTreeMissing),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, InvalidDefaultLocale) {
  SetupUnpacker("invalid_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(std::string(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, InvalidMessagesFile) {
  SetupUnpacker("invalid_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    std::string("*_locales?en_US?messages.json: Line: 2, column: 3,"
                " Dictionary keys must be quoted.")));
}

TEST_F(ExtensionUnpackerTest, MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(std::string(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(std::string(errors::kLocalesNoDefaultLocaleSpecified),
            unpacker_->error_message());
}

TEST_F(ExtensionUnpackerTest, MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    errors::kLocalesMessagesFileMissing +
    std::string("*_locales?en_US?messages.json")));
}

TEST_F(ExtensionUnpackerTest, NoLocaleData) {
  SetupUnpacker("no_locale_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(std::string(errors::kLocalesNoDefaultMessages),
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
