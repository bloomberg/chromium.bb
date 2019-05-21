// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/fake_registry.h"
#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_smb_provider_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

namespace chromeos {
namespace smb_client {

namespace {

const ProviderId kProviderId = ProviderId::CreateFromNativeId("smb");
const char kTestDomain[] = "EXAMPLE.COM";

void SaveMountResult(SmbMountResult* out, SmbMountResult result) {
  *out = result;
}

class MockSmbProviderClient : public chromeos::FakeSmbProviderClient {
 public:
  MockSmbProviderClient()
      : FakeSmbProviderClient(true /* should_run_synchronously */) {}

  MOCK_METHOD7(Mount,
               void(const base::FilePath& share_path,
                    bool ntlm_enabled,
                    const std::string& workgroup,
                    const std::string& username,
                    base::ScopedFD password_fd,
                    bool skip_connect,
                    SmbProviderClient::MountCallback callback));
  MOCK_METHOD2(SetupKerberos,
               void(const std::string& account_id,
                    SmbProviderClient::SetupKerberosCallback callback));
};

}  // namespace

class SmbServiceTest : public testing::Test {
 protected:
  SmbServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());

    std::unique_ptr<FakeChromeUserManager> user_manager_temp =
        std::make_unique<FakeChromeUserManager>();

    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_temp->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));

    ad_profile_ =
        profile_manager_->CreateTestingProfile("ad-test-user@example.com");
    user_manager_temp->AddUserWithAffiliationAndTypeAndProfile(
        AccountId::AdFromUserEmailObjGuid(ad_profile_->GetProfileUserName(),
                                          "abc"),
        false, user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY, ad_profile_);

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager_temp));

    mock_client_ = new MockSmbProviderClient;
    // The mock needs to be marked as leaked because ownership is passed to
    // DBusThreadManager.
    testing::Mock::AllowLeak(mock_client_);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSmbProviderClient(
        base::WrapUnique(mock_client_));
  }

  ~SmbServiceTest() override {}

  void CreateFspRegistry(TestingProfile* profile) {
    // Create FSP service.
    registry_ = new file_system_provider::FakeRegistry;
    file_system_provider::Service::Get(profile)->SetRegistryForTesting(
        base::WrapUnique(registry_));
  }

  void CreateService(TestingProfile* profile) {
    SmbService::DisableShareDiscoveryForTesting();

    // Create smb service.
    smb_service_ = std::make_unique<SmbService>(
        profile, std::make_unique<base::SimpleTestTickClock>());
  }

  void ExpectInvalidUrl(const std::string& url) {
    SmbMountResult result = SmbMountResult::SUCCESS;
    smb_service_->CallMount({} /* options */, base::FilePath(url),
                            "" /* username */, "" /* password */,
                            false /* use_chromad_kerberos */,
                            false /* should_open_file_manager_after_mount */,
                            base::BindOnce(&SaveMountResult, &result));
    EXPECT_EQ(result, SmbMountResult::INVALID_URL);
  }

  void ExpectInvalidSsoUrl(const std::string& url) {
    SmbMountResult result = SmbMountResult::SUCCESS;
    smb_service_->CallMount({} /* options */, base::FilePath(url),
                            "" /* username */, "" /* password */,
                            true /* use_chromad_kerberos */,
                            false /* should_open_file_manager_after_mount */,
                            base::BindOnce(&SaveMountResult, &result));
    EXPECT_EQ(result, SmbMountResult::INVALID_SSO_URL);
  }

  content::TestBrowserThreadBundle
      thread_bundle_;        // Included so tests magically don't crash.
  TestingProfile* profile_ = nullptr;     // Not owned.
  TestingProfile* ad_profile_ = nullptr;  // Not owned.
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  MockSmbProviderClient* mock_client_;  // Owned by DBusThreadManager.
  std::unique_ptr<SmbService> smb_service_;

  std::unique_ptr<file_system_provider::Service> fsp_service_;
  file_system_provider::FakeRegistry* registry_;  // Owned by |fsp_service_|.
  // Extension Registry and Registry needed for fsp_service.
  std::unique_ptr<extensions::ExtensionRegistry> extension_registry_;
};

TEST_F(SmbServiceTest, InvalidUrls) {
  CreateService(profile_);

  ExpectInvalidUrl("");
  ExpectInvalidUrl("foo");
  ExpectInvalidUrl("\\foo");
  ExpectInvalidUrl("\\\\foo");
  ExpectInvalidUrl("\\\\foo\\");
  ExpectInvalidUrl("file://foo/bar");
  ExpectInvalidUrl("smb://foo");
  ExpectInvalidUrl("smb://user@password:foo");
  ExpectInvalidUrl("smb:\\\\foo\\bar");
  ExpectInvalidUrl("//foo/bar");
}

TEST_F(SmbServiceTest, InvalidSsoUrls) {
  CreateService(profile_);

  ExpectInvalidSsoUrl("\\\\192.168.1.1\\foo");
  ExpectInvalidSsoUrl("\\\\[0:0:0:0:0:0:0:1]\\foo");
  ExpectInvalidSsoUrl("\\\\[::1]\\foo");
  ExpectInvalidSsoUrl("smb://192.168.1.1/foo");
  ExpectInvalidSsoUrl("smb://[0:0:0:0:0:0:0:1]/foo");
  ExpectInvalidSsoUrl("smb://[::1]/foo");
}

TEST_F(SmbServiceTest, Remount) {
  const char* kSharePath = "\\\\server\\foobar";
  const char* kRemountPath = "smb://server/foobar";

  file_system_provider::MountOptions mount_options(
      CreateFileSystemId(base::FilePath(kSharePath),
                         false /* is_kerberos_chromad */),
      "Foo");
  ProvidedFileSystemInfo file_system_info(
      kProviderId, mount_options, base::FilePath(kSharePath),
      false /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, chromeos::file_system_provider::IconSet());
  CreateFspRegistry(profile_);
  registry_->RememberFileSystem(file_system_info, {});

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_client_, Mount(base::FilePath(kRemountPath), _, "", "", _,
                                   true /* skip_connect */, _))
      .WillOnce(WithArg<6>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateService(profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Remount_ActiveDirectory) {
  const char* kSharePath = "\\\\krbserver\\foobar";
  const char* kRemountPath = "smb://krbserver/foobar";

  file_system_provider::MountOptions mount_options(
      CreateFileSystemId(base::FilePath(kSharePath),
                         true /* is_kerberos_chromad */),
      "Foo");
  ProvidedFileSystemInfo file_system_info(
      kProviderId, mount_options, base::FilePath(kSharePath),
      false /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, chromeos::file_system_provider::IconSet());
  CreateFspRegistry(ad_profile_);
  registry_->RememberFileSystem(file_system_info, {});

  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, SetupKerberos(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](SmbProviderClient::SetupKerberosCallback callback) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), true));
          })));
  EXPECT_CALL(*mock_client_,
              Mount(base::FilePath(kRemountPath), _, kTestDomain,
                    "ad-test-user", _, true /* skip_connect */, _))
      .WillOnce(WithArg<6>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateService(ad_profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Premount) {
  const char kPremountPath[] = "smb://server/foobar";
  const char kPreconfiguredShares[] =
      "[{\"mode\":\"pre_mount\",\"share_url\":\"\\\\\\\\server\\\\foobar\"}]";
  auto parsed_shares = base::JSONReader::Read(kPreconfiguredShares);
  ASSERT_TRUE(parsed_shares);
  profile_->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                            *parsed_shares);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_client_, Mount(base::FilePath(kPremountPath), _, "", "", _,
                                   true /* skip_connect */, _))
      .WillOnce(WithArg<6>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateFspRegistry(profile_);
  CreateService(profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

}  // namespace smb_client
}  // namespace chromeos
