// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

using testing::_;
using testing::Invoke;

void OnUnpackSuccess(const FilePath& temp_dir,
                     const FilePath& extension_root,
                     Extension* extension) {
  delete extension;
  // Don't delete temp_dir here, we need to do some post op checking.
}

class MockSandboxedExtensionUnpackerClient
    : public SandboxedExtensionUnpackerClient {
 public:
  virtual ~MockSandboxedExtensionUnpackerClient() {}

  MOCK_METHOD3(OnUnpackSuccess,
               void(const FilePath& temp_dir,
                    const FilePath& extension_root,
                    Extension* extension));

  MOCK_METHOD1(OnUnpackFailure,
               void(const std::string& error));

  void DelegateToFake() {
    ON_CALL(*this, OnUnpackSuccess(_, _, _))
        .WillByDefault(Invoke(::OnUnpackSuccess));
  }
};

class SandboxedExtensionUnpackerTest : public testing::Test {
 public:
  virtual void SetUp() {
    // It will delete itself.
    client_ = new MockSandboxedExtensionUnpackerClient;
    client_->DelegateToFake();
  }

  virtual void TearDown() {
    // Clean up finally.
    ASSERT_TRUE(file_util::Delete(install_dir_, true)) <<
        install_dir_.value();
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
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &install_dir_));
    install_dir_ =
      install_dir_.AppendASCII("sandboxed_extension_unpacker_test");
    file_util::Delete(install_dir_, true);
    file_util::CreateDirectory(install_dir_);

    FilePath crx_path = install_dir_.AppendASCII(crx_name);
    ASSERT_TRUE(file_util::CopyFile(original_path, crx_path)) <<
        "Original path: " << original_path.value() <<
        ", Crx path: " << crx_path.value();

    unpacker_.reset(new ExtensionUnpacker(crx_path));


    // Build a temp area where the extension will be unpacked.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &temp_dir_));
    temp_dir_ = temp_dir_.AppendASCII("sandboxed_extension_unpacker_test_Temp");
    file_util::CreateDirectory(temp_dir_);

    sandboxed_unpacker_ =
      new SandboxedExtensionUnpacker(crx_path, temp_dir_, NULL, client_);
    PrepareUnpackerEnv();
  }

  void PrepareUnpackerEnv() {
    sandboxed_unpacker_->extension_root_ =
      install_dir_.AppendASCII(extension_filenames::kTempExtensionName);

    sandboxed_unpacker_->temp_dir_.Set(install_dir_);
    sandboxed_unpacker_->public_key_ =
      "ocnapchkplbmjmpfehjocmjnipfmogkh";
  }

  void OnUnpackSucceeded() {
    sandboxed_unpacker_->OnUnpackExtensionSucceeded(
        *unpacker_->parsed_manifest());
  }

  FilePath GetInstallPath() {
    return install_dir_.AppendASCII(extension_filenames::kTempExtensionName);
  }

  bool TempFilesRemoved() {
    // Check that temporary files were cleaned up.
    file_util::FileEnumerator::FILE_TYPE files_and_dirs =
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
        file_util::FileEnumerator::DIRECTORIES |
        file_util::FileEnumerator::FILES);

    file_util::FileEnumerator temp_iterator(
      temp_dir_,
      true,  // recursive
      files_and_dirs
    );
    int items_not_removed = 0;
    FilePath item_in_temp;
    item_in_temp = temp_iterator.Next();
    while (!item_in_temp.value().empty()) {
      items_not_removed++;
      EXPECT_STREQ(FILE_PATH_LITERAL(""), item_in_temp.value().c_str())
        << "File was not removed on success.";
      item_in_temp = temp_iterator.Next();
    }
    return (items_not_removed == 0);
  }

 protected:
  FilePath install_dir_;
  FilePath temp_dir_;
  MockSandboxedExtensionUnpackerClient* client_;
  scoped_ptr<ExtensionUnpacker> unpacker_;
  scoped_refptr<SandboxedExtensionUnpacker> sandboxed_unpacker_;
};

TEST_F(SandboxedExtensionUnpackerTest, NoCatalogsSuccess) {
  EXPECT_CALL(*client_, OnUnpackSuccess(_, _, _));

  SetupUnpacker("no_l10n.crx");
  ASSERT_TRUE(unpacker_->Run());
  ASSERT_TRUE(unpacker_->DumpImagesToFile());
  ASSERT_TRUE(unpacker_->DumpMessageCatalogsToFile());

  // Check that there is no _locales folder.
  FilePath install_path =
    GetInstallPath().Append(Extension::kLocaleFolder);
  EXPECT_FALSE(file_util::PathExists(install_path));

  OnUnpackSucceeded();

  // Check that there still is no _locales folder.
  EXPECT_FALSE(file_util::PathExists(install_path));

  ASSERT_TRUE(TempFilesRemoved());
}

TEST_F(SandboxedExtensionUnpackerTest, WithCatalogsSuccess) {
  EXPECT_CALL(*client_, OnUnpackSuccess(_, _, _));

  SetupUnpacker("good_l10n.crx");
  ASSERT_TRUE(unpacker_->Run());
  ASSERT_TRUE(unpacker_->DumpImagesToFile());
  ASSERT_TRUE(unpacker_->DumpMessageCatalogsToFile());

  // Check timestamp on _locales/en_US/messages.json.
  FilePath messages_file;
  messages_file = GetInstallPath().Append(Extension::kLocaleFolder)
      .AppendASCII("en_US")
      .Append(Extension::kMessagesFilename);
  file_util::FileInfo old_info;
  EXPECT_TRUE(file_util::GetFileInfo(messages_file, &old_info));

  // unpacker_->Run unpacks the extension. OnUnpackSucceeded overwrites some
  // of the files. To force timestamp on overwriten files to be different we use
  // Sleep(1s). See comment on file_util::CountFilesCreatedAfter.
  PlatformThread::Sleep(1000);
  OnUnpackSucceeded();

  // Check that there is newer _locales/en_US/messages.json file.
  file_util::FileInfo new_info;
  EXPECT_TRUE(file_util::GetFileInfo(messages_file, &new_info));

  EXPECT_TRUE(new_info.last_modified > old_info.last_modified);

  ASSERT_TRUE(TempFilesRemoved());
}
