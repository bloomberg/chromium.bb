// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
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
  virtual void SetUp () {
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
    ASSERT_TRUE(file_util::Delete(install_dir_, true)) <<
        install_dir_.value();
    ASSERT_TRUE(file_util::CreateDirectory(install_dir_)) <<
        install_dir_.value();

    FilePath crx_path = install_dir_.AppendASCII(crx_name);
    ASSERT_TRUE(file_util::CopyFile(original_path, crx_path)) <<
        "Original path: " << original_path.value() <<
        ", Crx path: " << crx_path.value();

    unpacker_.reset(new ExtensionUnpacker(crx_path));

    // It will delete itself.
    sandboxed_unpacker_ =
        new SandboxedExtensionUnpacker(crx_path, NULL, client_);
    PrepareUnpackerEnv();
  }

  void PrepareUnpackerEnv() {
    sandboxed_unpacker_->extension_root_ =
      install_dir_.AppendASCII("TEMP_INSTALL");

    sandboxed_unpacker_->temp_dir_.Set(install_dir_);
    sandboxed_unpacker_->public_key_ =
      "ocnapchkplbmjmpfehjocmjnipfmogkh";
  }

  void OnUnpackSucceeded() {
    sandboxed_unpacker_->OnUnpackExtensionSucceeded(
        *unpacker_->parsed_manifest(),
        *unpacker_->parsed_catalogs());
  }

  FilePath GetInstallPath() {
    return install_dir_.AppendASCII("TEMP_INSTALL");
  }

 protected:
  FilePath install_dir_;
  MockSandboxedExtensionUnpackerClient* client_;
  scoped_ptr<ExtensionUnpacker> unpacker_;
  scoped_refptr<SandboxedExtensionUnpacker> sandboxed_unpacker_;
};

TEST_F(SandboxedExtensionUnpackerTest, NoCatalogsSuccess) {
  EXPECT_CALL(*client_, OnUnpackSuccess(_, _, _));

  SetupUnpacker("no_l10n.crx");
  ASSERT_TRUE(unpacker_->Run());
  ASSERT_TRUE(unpacker_->DumpImagesToFile());

  // Check that there is no _locales folder.
  FilePath install_path =
    GetInstallPath().AppendASCII(Extension::kLocaleFolder);
  EXPECT_FALSE(file_util::PathExists(install_path));

  OnUnpackSucceeded();

  // Check that there still is no _locales folder.
  EXPECT_FALSE(file_util::PathExists(install_path));
}

TEST_F(SandboxedExtensionUnpackerTest, WithCatalogsSuccess) {
  EXPECT_CALL(*client_, OnUnpackSuccess(_, _, _));

  SetupUnpacker("good_l10n.crx");
  ASSERT_TRUE(unpacker_->Run());
  ASSERT_TRUE(unpacker_->DumpImagesToFile());

  // Delete _locales/en_US/messages.json.
  FilePath messages_file;
  messages_file = GetInstallPath()
      .AppendASCII(Extension::kLocaleFolder)
      .AppendASCII("en_US")
      .AppendASCII(Extension::kMessagesFilename);
  EXPECT_TRUE(file_util::Delete(messages_file, false));

  OnUnpackSucceeded();

  // Check that there is _locales/en_US/messages.json file.
  EXPECT_TRUE(file_util::PathExists(messages_file));
}
