// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wallpaper/wallpaper_controller_observer.h"
#include "ash/common/wm_shell.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/rsa_private_key.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kRedImageFileName[] = "chromeos/wallpapers/red.jpg";
const char kGreenImageFileName[] = "chromeos/wallpapers/green.jpg";
const char kBlueImageFileName[] = "chromeos/wallpapers/blue.jpg";

const SkColor kRedImageColor = SkColorSetARGB(255, 199, 6, 7);
const SkColor kGreenImageColor = SkColorSetARGB(255, 38, 196, 15);

policy::CloudPolicyStore* GetStoreForUser(const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  if (!profile) {
    ADD_FAILURE();
    return NULL;
  }
  policy::UserCloudPolicyManagerChromeOS* policy_manager =
      policy::UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          profile);
  if (!policy_manager) {
    ADD_FAILURE();
    return NULL;
  }
  return policy_manager->core()->store();
}

// Compute the average ARGB color of |bitmap|.
SkColor ComputeAverageColor(const SkBitmap& bitmap) {
  if (bitmap.empty() || bitmap.width() < 1 || bitmap.height() < 1) {
    ADD_FAILURE() << "Empty or invalid bitmap.";
    return SkColorSetARGB(0, 0, 0, 0);
  }
  if (bitmap.isNull()) {
    ADD_FAILURE() << "Bitmap has no pixelref.";
    return SkColorSetARGB(0, 0, 0, 0);
  }
  if (bitmap.colorType() == kUnknown_SkColorType) {
    ADD_FAILURE() << "Bitmap has not been configured.";
    return SkColorSetARGB(0, 0, 0, 0);
  }
  uint64_t a = 0, r = 0, g = 0, b = 0;
  bitmap.lockPixels();
  for (int x = 0; x < bitmap.width(); ++x) {
    for (int y = 0; y < bitmap.height(); ++y) {
      const SkColor color = bitmap.getColor(x, y);
      a += SkColorGetA(color);
      r += SkColorGetR(color);
      g += SkColorGetG(color);
      b += SkColorGetB(color);
    }
  }
  bitmap.unlockPixels();
  uint64_t pixel_number = bitmap.width() * bitmap.height();
  return SkColorSetARGB((a + pixel_number / 2) / pixel_number,
                        (r + pixel_number / 2) / pixel_number,
                        (g + pixel_number / 2) / pixel_number,
                        (b + pixel_number / 2) / pixel_number);
}

// Obtain wallpaper image and return its average ARGB color.
SkColor GetAverageWallpaperColor() {
  const gfx::ImageSkia image =
      ash::WmShell::Get()->wallpaper_controller()->GetWallpaper();

  const gfx::ImageSkiaRep& representation = image.GetRepresentation(1.);
  if (representation.is_null()) {
    ADD_FAILURE() << "No image representation.";
    return SkColorSetARGB(0, 0, 0, 0);
  }

  const SkBitmap& bitmap = representation.sk_bitmap();
  return ComputeAverageColor(bitmap);
}

// Initialize system salt to calculate wallpaper file names.
void SetSystemSalt() {
  chromeos::SystemSaltGetter::Get()->SetRawSaltForTesting(
      chromeos::SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));
}

}  // namespace

class WallpaperManagerPolicyTest : public LoginManagerTest,
                                   public ash::WallpaperControllerObserver {
 protected:
  WallpaperManagerPolicyTest()
      : LoginManagerTest(true),
        wallpaper_change_count_(0),
        owner_key_util_(new ownership::MockOwnerKeyUtil()),
        fake_session_manager_client_(new FakeSessionManagerClient) {
    testUsers_.push_back(
        AccountId::FromUserEmail(LoginManagerTest::kEnterpriseUser1));
    testUsers_.push_back(
        AccountId::FromUserEmail(LoginManagerTest::kEnterpriseUser2));
  }

  std::unique_ptr<policy::UserPolicyBuilder> GetUserPolicyBuilder(
      const AccountId& account_id) {
    std::unique_ptr<policy::UserPolicyBuilder> user_policy_builder(
        new policy::UserPolicyBuilder());
    base::FilePath user_keys_dir;
    EXPECT_TRUE(PathService::Get(DIR_USER_POLICY_KEYS, &user_keys_dir));
    const std::string sanitized_user_id =
        CryptohomeClient::GetStubSanitizedUsername(
            cryptohome::Identification(account_id));
    const base::FilePath user_key_file =
        user_keys_dir.AppendASCII(sanitized_user_id)
                     .AppendASCII("policy.pub");
    std::string user_key_bits =
        user_policy_builder->GetPublicSigningKeyAsString();
    EXPECT_FALSE(user_key_bits.empty());
    EXPECT_TRUE(base::CreateDirectory(user_key_file.DirName()));
    EXPECT_EQ(base::WriteFile(user_key_file, user_key_bits.data(),
                              user_key_bits.length()),
              base::checked_cast<int>(user_key_bits.length()));
    user_policy_builder->policy_data().set_username(account_id.GetUserEmail());
    return user_policy_builder;
  }

  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    device_policy_.Build();
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    fake_session_manager_client_->set_device_policy(device_policy_.GetBlob());
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));

    // Set up fake install attributes.
    std::unique_ptr<chromeos::StubInstallAttributes> attributes =
        base::MakeUnique<chromeos::StubInstallAttributes>();
    attributes->SetEnterprise("fake-domain", "fake-id");
    policy::BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
        attributes.release());

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the same switches as LoginManagerTest, except that kMultiProfiles is
    // only set when GetParam() is true and except that kLoginProfile is set
    // when GetParam() is false.  The latter seems to be required for the sane
    // start-up of user profiles.
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);

    // Allow policy fetches to fail - these tests instead invoke InjectPolicy()
    // to directly inject and modify policy dynamically.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);

    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    ash::WmShell::Get()->wallpaper_controller()->AddObserver(this);

    // Set up policy signing.
    user_policy_builders_[0] = GetUserPolicyBuilder(testUsers_[0]);
    user_policy_builders_[1] = GetUserPolicyBuilder(testUsers_[1]);
  }

  void TearDownOnMainThread() override {
    ash::WmShell::Get()->wallpaper_controller()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // ash::WallpaperControllerObserver:
  void OnWallpaperDataChanged() override {
    ++wallpaper_change_count_;
    if (run_loop_)
      run_loop_->Quit();
  }

  // Runs the loop until wallpaper has changed at least |count| times in total.
  void RunUntilWallpaperChangeCount(int count) {
    while (wallpaper_change_count_ < count) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
    }
  }

  std::string ConstructPolicy(const std::string& relative_path) const {
    std::string image_data;
    if (!base::ReadFileToString(test_data_dir_.Append(relative_path),
                                &image_data)) {
      ADD_FAILURE();
    }
    std::string policy;
    base::JSONWriter::Write(*policy::test::ConstructExternalDataReference(
                                embedded_test_server()
                                    ->GetURL(std::string("/") + relative_path)
                                    .spec(),
                                image_data),
                            &policy);
    return policy;
  }

  // Inject |filename| as wallpaper policy for test user |user_number|.  Set
  // empty |filename| to clear policy.
  void InjectPolicy(int user_number, const std::string& filename) {
    ASSERT_TRUE(user_number == 0 || user_number == 1);
    const AccountId& account_id = testUsers_[user_number];
    policy::UserPolicyBuilder* builder =
        user_policy_builders_[user_number].get();
    if (!filename.empty()) {
      builder->payload().
          mutable_wallpaperimage()->set_value(ConstructPolicy(filename));
    } else {
      builder->payload().Clear();
    }
    builder->Build();
    fake_session_manager_client_->set_user_policy(
        cryptohome::Identification(account_id), builder->GetBlob());
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    ASSERT_TRUE(user);
    policy::CloudPolicyStore* store = GetStoreForUser(user);
    ASSERT_TRUE(store);
    store->Load();
    ASSERT_EQ(policy::CloudPolicyStore::STATUS_OK, store->status());
    ASSERT_EQ(policy::CloudPolicyValidatorBase::VALIDATION_OK,
              store->validation_status());
  }

  // Obtain WallpaperInfo for |user_number| from WallpaperManager.
  void GetUserWallpaperInfo(int user_number,
                            wallpaper::WallpaperInfo* wallpaper_info) {
    WallpaperManager::Get()->GetUserWallpaperInfo(testUsers_[user_number],
                                                  wallpaper_info);
  }

  base::FilePath test_data_dir_;
  std::unique_ptr<base::RunLoop> run_loop_;
  int wallpaper_change_count_;
  std::unique_ptr<policy::UserPolicyBuilder> user_policy_builders_[2];
  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  FakeSessionManagerClient* fake_session_manager_client_;
  std::vector<AccountId> testUsers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerPolicyTest);
};

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PRE_SetResetClear) {
  RegisterUser(testUsers_[0].GetUserEmail());
  RegisterUser(testUsers_[1].GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

// Verifies that the wallpaper can be set and re-set through policy and that
// setting policy for a user that is not logged in doesn't affect the current
// user.  Also verifies that after the policy has been cleared, the wallpaper
// reverts to default.
IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, SetResetClear) {
  SetSystemSalt();
  wallpaper::WallpaperInfo info;
  LoginUser(testUsers_[0].GetUserEmail());
  base::RunLoop().RunUntilIdle();

  // First user: Wait until default wallpaper has been loaded (happens
  // automatically) and store color to recognize it later.
  RunUntilWallpaperChangeCount(1);
  const SkColor original_wallpaper_color = GetAverageWallpaperColor();

  // Second user: Set wallpaper policy to blue image.  This should not result in
  // a wallpaper change, which is checked at the very end of this test.
  InjectPolicy(1, kBlueImageFileName);

  // First user: Set wallpaper policy to red image and verify average color.
  InjectPolicy(0, kRedImageFileName);
  RunUntilWallpaperChangeCount(2);
  GetUserWallpaperInfo(0, &info);
  ASSERT_EQ(user_manager::User::POLICY, info.type);
  ASSERT_EQ(kRedImageColor, GetAverageWallpaperColor());

  // First user: Set wallpaper policy to green image and verify average color.
  InjectPolicy(0, kGreenImageFileName);
  RunUntilWallpaperChangeCount(3);
  GetUserWallpaperInfo(0, &info);
  ASSERT_EQ(user_manager::User::POLICY, info.type);
  ASSERT_EQ(kGreenImageColor, GetAverageWallpaperColor());

  // First user: Clear wallpaper policy and verify that the default wallpaper is
  // set again.
  InjectPolicy(0, "");
  RunUntilWallpaperChangeCount(4);
  GetUserWallpaperInfo(0, &info);
  ASSERT_EQ(user_manager::User::DEFAULT, info.type);
  ASSERT_EQ(original_wallpaper_color, GetAverageWallpaperColor());

  // Check wallpaper change count to ensure that setting the second user's
  // wallpaper didn't have any effect.
  ASSERT_EQ(4, wallpaper_change_count_);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_PRE_PRE_PRE_WallpaperOnLoginScreen) {
  RegisterUser(testUsers_[0].GetUserEmail());
  RegisterUser(testUsers_[1].GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_PRE_PRE_WallpaperOnLoginScreen) {
  LoginUser(testUsers_[0].GetUserEmail());

  // Wait until default wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);

  // Set wallpaper policy to red image.
  InjectPolicy(0, kRedImageFileName);

  // Run until wallpaper has changed.
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kRedImageColor, GetAverageWallpaperColor());
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_PRE_WallpaperOnLoginScreen) {
  LoginUser(testUsers_[1].GetUserEmail());

  // Wait until default wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);

  // Set wallpaper policy to green image.
  InjectPolicy(1, kGreenImageFileName);

  // Run until wallpaper has changed.
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kGreenImageColor, GetAverageWallpaperColor());
}

// Disabled due to flakiness: http://crbug.com/385648.
IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_WallpaperOnLoginScreen) {
  // Wait for active pod's wallpaper to be loaded.
  RunUntilWallpaperChangeCount(1);
  ASSERT_EQ(kGreenImageColor, GetAverageWallpaperColor());

  // Select the second pod (belonging to user 1).
  ASSERT_TRUE(content::ExecuteScript(
      LoginDisplayHost::default_host()->GetOobeUI()->web_ui()->GetWebContents(),
      "document.getElementsByClassName('pod')[1].focus();"));
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kRedImageColor, GetAverageWallpaperColor());
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PRE_PRE_PersistOverLogout) {
  SetSystemSalt();
  RegisterUser(testUsers_[0].GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PRE_PersistOverLogout) {
  SetSystemSalt();
  LoginUser(testUsers_[0].GetUserEmail());

  // Wait until default wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);

  // Set wallpaper policy to red image.
  InjectPolicy(0, kRedImageFileName);

  // Run until wallpaper has changed.
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kRedImageColor, GetAverageWallpaperColor());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PersistOverLogout) {
  LoginUser(testUsers_[0].GetUserEmail());

  // Wait until wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);
  ASSERT_EQ(kRedImageColor, GetAverageWallpaperColor());
}

}  // namespace chromeos
