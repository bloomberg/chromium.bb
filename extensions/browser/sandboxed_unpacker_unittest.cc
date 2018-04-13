// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/sandboxed_unpacker.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/services/unzip/public/cpp/test_unzip_service.h"
#include "components/services/unzip/unzip_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "extensions/test/test_extensions_client.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/cpp/test_data_decoder_service.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Inserts an illegal path into the browser images returned by
// TestExtensionsClient for any extension.
class IllegalImagePathInserter
    : public TestExtensionsClient::BrowserImagePathsFilter {
 public:
  IllegalImagePathInserter(TestExtensionsClient* client) : client_(client) {
    client_->AddBrowserImagePathsFilter(this);
  }

  virtual ~IllegalImagePathInserter() {
    client_->RemoveBrowserImagePathsFilter(this);
  }

  void Filter(const Extension* extension,
              std::set<base::FilePath>* paths) override {
    base::FilePath illegal_path =
        base::FilePath(base::FilePath::kParentDirectory)
            .AppendASCII(kTempExtensionName)
            .AppendASCII("product_logo_128.png");
    paths->insert(illegal_path);
  }

 private:
  TestExtensionsClient* client_;
};

}  // namespace

class MockSandboxedUnpackerClient : public SandboxedUnpackerClient {
 public:
  void WaitForUnpack() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    quit_closure_ = runner->QuitClosure();
    runner->Run();
  }

  base::FilePath temp_dir() const { return temp_dir_; }
  base::string16 unpack_err() const { return error_; }

  void set_deleted_tracker(bool* deleted_tracker) {
    deleted_tracker_ = deleted_tracker;
  }

 private:
  ~MockSandboxedUnpackerClient() override {
    if (deleted_tracker_)
      *deleted_tracker_ = true;
  }

  void OnUnpackSuccess(
      const base::FilePath& temp_dir,
      const base::FilePath& extension_root,
      std::unique_ptr<base::DictionaryValue> original_manifest,
      const Extension* extension,
      const SkBitmap& install_icon,
      const base::Optional<int>& dnr_ruleset_checksum) override {
    temp_dir_ = temp_dir;
    quit_closure_.Run();
  }

  void OnUnpackFailure(const CrxInstallError& error) override {
    error_ = error.message();
    quit_closure_.Run();
  }

  base::string16 error_;
  base::Closure quit_closure_;
  base::FilePath temp_dir_;
  bool* deleted_tracker_ = nullptr;
};

class SandboxedUnpackerTest : public ExtensionsTest {
 public:
  SandboxedUnpackerTest()
      : SandboxedUnpackerTest(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  SandboxedUnpackerTest(content::TestBrowserThreadBundle::Options options)
      : ExtensionsTest(options) {}

  void SetUp() override {
    // TODO(devlin): Remove this. See https://crbug.com/816679.
    allow_legacy_extensions_ = Extension::allow_legacy_extensions_for_testing();

    ExtensionsTest::SetUp();
    ASSERT_TRUE(extensions_dir_.CreateUniqueTempDir());
    in_process_utility_thread_helper_.reset(
        new content::InProcessUtilityThreadHelper);
    // It will delete itself.
    client_ = new MockSandboxedUnpackerClient;

    InitSanboxedUnpacker(/*data_decode_service=*/nullptr,
                         /*unzip_service=*/nullptr);
  }

  void InitSanboxedUnpacker(
      std::unique_ptr<service_manager::Service> data_decode_service,
      std::unique_ptr<service_manager::Service> unzip_service) {
    service_manager::TestConnectorFactory::NameToServiceMap services;
    if (!data_decode_service)
      data_decode_service = data_decoder::DataDecoderService::Create();
    if (!unzip_service)
      unzip_service = unzip::UnzipService::CreateService();
    services.insert(
        std::make_pair("data_decoder", std::move(data_decode_service)));
    services.insert(std::make_pair("unzip_service", std::move(unzip_service)));
    test_connector_factory_ =
        service_manager::TestConnectorFactory::CreateForServices(
            std::move(services));
    connector_ = test_connector_factory_->CreateConnector();

    sandboxed_unpacker_ =
        new SandboxedUnpacker(connector_->Clone(), Manifest::INTERNAL,
                              Extension::NO_FLAGS, extensions_dir_.GetPath(),
                              base::ThreadTaskRunnerHandle::Get(), client_);
  }

  void TearDown() override {
    // Need to destruct SandboxedUnpacker before the message loop since
    // it posts a task to it.
    sandboxed_unpacker_ = nullptr;
    base::RunLoop().RunUntilIdle();
    ExtensionsTest::TearDown();
    in_process_utility_thread_helper_.reset();
    allow_legacy_extensions_.reset();
  }

  base::FilePath GetCrxFullPath(const std::string& crx_name) {
    base::FilePath full_path;
    EXPECT_TRUE(PathService::Get(extensions::DIR_TEST_DATA, &full_path));
    full_path = full_path.AppendASCII("unpacker").AppendASCII(crx_name);
    EXPECT_TRUE(base::PathExists(full_path)) << full_path.value();
    return full_path;
  }

  void SetupUnpacker(const std::string& crx_name,
                     const std::string& package_hash) {
    base::FilePath crx_path = GetCrxFullPath(crx_name);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            &SandboxedUnpacker::StartWithCrx, sandboxed_unpacker_,
            extensions::CRXFileInfo(std::string(), crx_path, package_hash)));
    client_->WaitForUnpack();
  }

  void SetupUnpackerWithDirectory(const std::string& crx_name) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath crx_path = GetCrxFullPath(crx_name);
    ASSERT_TRUE(zip::Unzip(crx_path, temp_dir.GetPath()));

    std::string fake_id = crx_file::id_util::GenerateId(crx_name);
    std::string fake_public_key;
    base::Base64Encode(std::string(2048, 'k'), &fake_public_key);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&SandboxedUnpacker::StartWithDirectory, sandboxed_unpacker_,
                   fake_id, fake_public_key, temp_dir.Take()));
    client_->WaitForUnpack();
  }

  bool InstallSucceeded() const { return !client_->temp_dir().empty(); }

  base::FilePath GetInstallPath() const {
    return client_->temp_dir().AppendASCII(kTempExtensionName);
  }

  base::string16 GetInstallError() const { return client_->unpack_err(); }

  void ExpectInstallErrorContains(const std::string& error) {
    std::string full_error = base::UTF16ToUTF8(client_->unpack_err());
    EXPECT_TRUE(full_error.find(error) != std::string::npos)
        << "Error message " << full_error << " does not contain " << error;
  }

  // Unpacks the package |package_name| and checks that |sandboxed_unpacker_|
  // gets deleted.
  void TestSandboxedUnpackerDeleted(const std::string& package_name,
                                    bool expect_success) {
    bool client_deleted = false;
    client_->set_deleted_tracker(&client_deleted);
    SetupUnpacker(package_name, "");
    EXPECT_EQ(GetInstallError().empty(), expect_success);
    // Remove our reference to |sandboxed_unpacker_|, it should get deleted
    // since/ it's the last reference.
    sandboxed_unpacker_ = nullptr;
    // The SandboxedUnpacker should have been deleted and deleted the client.
    EXPECT_TRUE(client_deleted);
  }

 protected:
  base::ScopedTempDir extensions_dir_;
  MockSandboxedUnpackerClient* client_;
  scoped_refptr<SandboxedUnpacker> sandboxed_unpacker_;
  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  std::unique_ptr<service_manager::TestConnectorFactory>
      test_connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  Extension::ScopedAllowLegacyExtensions allow_legacy_extensions_;
};

TEST_F(SandboxedUnpackerTest, EmptyDefaultLocale) {
  SetupUnpacker("empty_default_locale.crx", "");
  ExpectInstallErrorContains(manifest_errors::kInvalidDefaultLocale);
}

TEST_F(SandboxedUnpackerTest, HasDefaultLocaleMissingLocalesFolder) {
  SetupUnpacker("has_default_missing_locales.crx", "");
  ExpectInstallErrorContains(manifest_errors::kLocalesTreeMissing);
}

TEST_F(SandboxedUnpackerTest, InvalidDefaultLocale) {
  SetupUnpacker("invalid_default_locale.crx", "");
  ExpectInstallErrorContains(manifest_errors::kInvalidDefaultLocale);
}

TEST_F(SandboxedUnpackerTest, MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx", "");
  ExpectInstallErrorContains(manifest_errors::kLocalesNoDefaultMessages);
}

TEST_F(SandboxedUnpackerTest, MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx", "");
  ExpectInstallErrorContains(l10n_util::GetStringUTF8(
      IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED));
}

TEST_F(SandboxedUnpackerTest, MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx", "");
  EXPECT_TRUE(base::MatchPattern(
      GetInstallError(),
      base::ASCIIToUTF16("*") +
          base::ASCIIToUTF16(manifest_errors::kLocalesMessagesFileMissing) +
          base::ASCIIToUTF16("*_locales?en_US?messages.json'.")))
      << GetInstallError();
}

TEST_F(SandboxedUnpackerTest, NoLocaleData) {
  SetupUnpacker("no_locale_data.crx", "");
  ExpectInstallErrorContains(manifest_errors::kLocalesNoDefaultMessages);
}

TEST_F(SandboxedUnpackerTest, ImageDecodingError) {
  const char kExpected[] = "Could not decode image: ";
  SetupUnpacker("bad_image.crx", "");
  EXPECT_TRUE(base::StartsWith(GetInstallError(), base::ASCIIToUTF16(kExpected),
                               base::CompareCase::INSENSITIVE_ASCII))
      << "Expected prefix: \"" << kExpected << "\", actual error: \""
      << GetInstallError() << "\"";
}

TEST_F(SandboxedUnpackerTest, BadPathError) {
  IllegalImagePathInserter inserter(
      static_cast<TestExtensionsClient*>(ExtensionsClient::Get()));
  SetupUnpacker("good_package.crx", "");
  // Install should have failed with an error.
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallError().empty());
}

TEST_F(SandboxedUnpackerTest, NoCatalogsSuccess) {
  SetupUnpacker("no_l10n.crx", "");
  // Check that there is no _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
}

TEST_F(SandboxedUnpackerTest, FromDirNoCatalogsSuccess) {
  SetupUnpackerWithDirectory("no_l10n.crx");
  // Check that there is no _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
}

TEST_F(SandboxedUnpackerTest, WithCatalogsSuccess) {
  SetupUnpacker("good_l10n.crx", "");
  // Check that there is _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_TRUE(base::PathExists(install_path));
}

TEST_F(SandboxedUnpackerTest, FromDirWithCatalogsSuccess) {
  SetupUnpackerWithDirectory("good_l10n.crx");
  // Check that there is _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_TRUE(base::PathExists(install_path));
}

TEST_F(SandboxedUnpackerTest, FailHashCheck) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableCrxHashCheck);
  SetupUnpacker("good_l10n.crx", std::string(64, '0'));
  // Check that there is an error message.
  EXPECT_NE(base::string16(), GetInstallError());
}

TEST_F(SandboxedUnpackerTest, InvalidMessagesFile) {
  SetupUnpackerWithDirectory("invalid_messages_file.crx");
  // Check that there is no _locales folder.
  base::FilePath install_path = GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
  EXPECT_TRUE(base::MatchPattern(
      GetInstallError(),
      base::ASCIIToUTF16("*_locales?en_US?messages.json': Line: 2, column: 10,"
                         " Syntax error.'.")))
      << GetInstallError();
}

TEST_F(SandboxedUnpackerTest, PassHashCheck) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableCrxHashCheck);
  SetupUnpacker(
      "good_l10n.crx",
      "6fa171c726373785aa4fcd2df448c3db0420a95d5044fbee831f089b979c4068");
  // Check that there is no error message.
  EXPECT_EQ(base::string16(), GetInstallError());
}

TEST_F(SandboxedUnpackerTest, SkipHashCheck) {
  SetupUnpacker("good_l10n.crx", "badhash");
  // Check that there is no error message.
  EXPECT_EQ(base::string16(), GetInstallError());
}

// The following tests simulate the utility services failling.
TEST_F(SandboxedUnpackerTest, UnzipperServiceFails) {
  InitSanboxedUnpacker(
      /*data_decoder_service=*/nullptr,
      std::make_unique<unzip::CrashyUnzipService>());
  SetupUnpacker("good_package.crx", "");
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallError().empty());
}

TEST_F(SandboxedUnpackerTest, JsonParserFails) {
  InitSanboxedUnpacker(std::make_unique<data_decoder::CrashyDataDecoderService>(
                           /*crash_json=*/true, /*crash_image=*/false),
                       /*unzip_service=*/nullptr);
  SetupUnpacker("good_package.crx", "");
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallError().empty());
}

TEST_F(SandboxedUnpackerTest, ImageDecoderFails) {
  InitSanboxedUnpacker(std::make_unique<data_decoder::CrashyDataDecoderService>(
                           /*crash_json=*/false, /*crash_image=*/true),
                       /*unzip_service=*/nullptr);
  SetupUnpacker("good_package.crx", "");
  EXPECT_FALSE(InstallSucceeded());
  EXPECT_FALSE(GetInstallError().empty());
}

// SandboxedUnpacker is ref counted and is reference by callbacks and
// InterfacePtrs. This tests that it gets deleted as expected (so that no extra
// refs are left).
TEST_F(SandboxedUnpackerTest, DeletedOnSuccess) {
  TestSandboxedUnpackerDeleted("good_l10n.crx", /*expect_success=*/true);
}

TEST_F(SandboxedUnpackerTest, DeletedOnFailure) {
  TestSandboxedUnpackerDeleted("bad_image.crx", /*expect_success=*/false);
}

}  // namespace extensions
