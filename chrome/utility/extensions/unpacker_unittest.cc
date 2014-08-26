// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/utility/extensions/unpacker.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::ASCIIToUTF16;

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class UnpackerTest : public testing::Test {
 public:
  virtual ~UnpackerTest() {
    LOG(WARNING) << "Deleting temp dir: "
                 << temp_dir_.path().LossyDisplayName();
    LOG(WARNING) << temp_dir_.Delete();
  }

  void SetupUnpacker(const std::string& crx_name) {
    base::FilePath original_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &original_path));
    original_path = original_path.AppendASCII("extensions")
        .AppendASCII("unpacker")
        .AppendASCII(crx_name);
    ASSERT_TRUE(base::PathExists(original_path)) << original_path.value();

    // Try bots won't let us write into DIR_TEST_DATA, so we have to create
    // a temp folder to play in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath crx_path = temp_dir_.path().AppendASCII(crx_name);
    ASSERT_TRUE(base::CopyFile(original_path, crx_path)) <<
        "Original path " << original_path.value() <<
        ", Crx path " << crx_path.value();

    unpacker_.reset(new Unpacker(crx_path,
                                 std::string(),
                                 Manifest::INTERNAL,
                                 Extension::NO_FLAGS));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<Unpacker> unpacker_;
};

TEST_F(UnpackerTest, EmptyDefaultLocale) {
  SetupUnpacker("empty_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, HasDefaultLocaleMissingLocalesFolder) {
  SetupUnpacker("has_default_missing_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesTreeMissing),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, InvalidDefaultLocale) {
  SetupUnpacker("invalid_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, InvalidMessagesFile) {
  SetupUnpacker("invalid_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    ASCIIToUTF16("*_locales?en_US?messages.json: Line: 2, column: 11,"
        " Syntax error."))) << unpacker_->error_message();
}

TEST_F(UnpackerTest, MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultLocaleSpecified),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(MatchPattern(unpacker_->error_message(),
    ASCIIToUTF16(errors::kLocalesMessagesFileMissing) +
    ASCIIToUTF16("*_locales?en_US?messages.json")));
}

TEST_F(UnpackerTest, NoLocaleData) {
  SetupUnpacker("no_locale_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, GoodL10n) {
  SetupUnpacker("good_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  ASSERT_EQ(2U, unpacker_->parsed_catalogs()->size());
}

TEST_F(UnpackerTest, NoL10n) {
  SetupUnpacker("no_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  EXPECT_EQ(0U, unpacker_->parsed_catalogs()->size());
}

TEST_F(UnpackerTest, UnzipDirectoryError) {
  const char* kExpected = "Could not create directory for unzipping: ";
  SetupUnpacker("good_package.crx");
  base::FilePath path =
      temp_dir_.path().AppendASCII(kTempExtensionName);
  ASSERT_TRUE(base::WriteFile(path, "foo", 3));
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(StartsWith(unpacker_->error_message(),
              ASCIIToUTF16(kExpected),
              false)) << "Expected prefix: \"" << kExpected
                      << "\", actual error: \"" << unpacker_->error_message()
                      << "\"";
}

TEST_F(UnpackerTest, UnzipError) {
  const char* kExpected = "Could not unzip extension";
  SetupUnpacker("bad_zip.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(kExpected), unpacker_->error_message());
}

TEST_F(UnpackerTest, BadPathError) {
  const char* kExpected = "Illegal path (absolute or relative with '..'): ";
  SetupUnpacker("bad_path.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(StartsWith(unpacker_->error_message(),
              ASCIIToUTF16(kExpected),
              false)) << "Expected prefix: \"" << kExpected
                      << "\", actual error: \"" << unpacker_->error_message()
                      << "\"";
}


TEST_F(UnpackerTest, ImageDecodingError) {
  const char* kExpected = "Could not decode image: ";
  SetupUnpacker("bad_image.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(StartsWith(unpacker_->error_message(),
              ASCIIToUTF16(kExpected),
              false)) << "Expected prefix: \"" << kExpected
                      << "\", actual error: \"" << unpacker_->error_message()
                      << "\"";
}

}  // namespace extensions
