// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/shell.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/rsa_private_key.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "policy/proto/cloud_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kTestUsers[2][19] = { "test-0@example.com", "test-1@example.com" };

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
      policy::UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile);
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
  uint64 a = 0, r = 0, g = 0, b = 0;
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
  uint64 pixel_number = bitmap.width() * bitmap.height();
  return SkColorSetARGB((a + pixel_number / 2) / pixel_number,
                        (r + pixel_number / 2) / pixel_number,
                        (g + pixel_number / 2) / pixel_number,
                        (b + pixel_number / 2) / pixel_number);
}

// Obtain background image and return its average ARGB color.
SkColor GetAverageBackgroundColor() {
  const gfx::ImageSkia image =
      ash::Shell::GetInstance()->desktop_background_controller()->
      GetWallpaper();

  const gfx::ImageSkiaRep& representation = image.GetRepresentation(1.);
  if (representation.is_null()) {
    ADD_FAILURE() << "No image representation.";
    return SkColorSetARGB(0, 0, 0, 0);
  }

  const SkBitmap& bitmap = representation.sk_bitmap();
  return ComputeAverageColor(bitmap);
}

}  // namespace

class WallpaperManagerPolicyTest
    : public LoginManagerTest,
      public ash::DesktopBackgroundControllerObserver {
 protected:
  WallpaperManagerPolicyTest()
      : LoginManagerTest(true),
        wallpaper_change_count_(0),
        fake_dbus_thread_manager_(new FakeDBusThreadManager),
        fake_session_manager_client_(new FakeSessionManagerClient) {
    fake_dbus_thread_manager_->SetFakeClients();
    fake_dbus_thread_manager_->SetSessionManagerClient(
        scoped_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  scoped_ptr<policy::UserPolicyBuilder> GetUserPolicyBuilder(
      const std::string& user_id) {
    scoped_ptr<policy::UserPolicyBuilder>
        user_policy_builder(new policy::UserPolicyBuilder());
    base::FilePath user_keys_dir;
    EXPECT_TRUE(PathService::Get(DIR_USER_POLICY_KEYS, &user_keys_dir));
    const std::string sanitized_user_id =
        CryptohomeClient::GetStubSanitizedUsername(user_id);
    const base::FilePath user_key_file =
        user_keys_dir.AppendASCII(sanitized_user_id)
                     .AppendASCII("policy.pub");
    std::vector<uint8> user_key_bits;
    EXPECT_TRUE(user_policy_builder->GetSigningKey()->
                ExportPublicKey(&user_key_bits));
    EXPECT_TRUE(base::CreateDirectory(user_key_file.DirName()));
    EXPECT_EQ(base::WriteFile(
                  user_key_file,
                  reinterpret_cast<const char*>(user_key_bits.data()),
                  user_key_bits.size()),
              static_cast<int>(user_key_bits.size()));
    user_policy_builder->policy_data().set_username(user_id);
    return user_policy_builder.Pass();
  }

  // LoginManagerTest:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DBusThreadManager::SetInstanceForTesting(fake_dbus_thread_manager_);
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Set the same switches as LoginManagerTest, except that kMultiProfiles is
    // only set when GetParam() is true and except that kLoginProfile is set
    // when GetParam() is false.  The latter seems to be required for the sane
    // start-up of user profiles.
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    LoginManagerTest::SetUpOnMainThread();
    ash::Shell::GetInstance()->
        desktop_background_controller()->AddObserver(this);

    // Set up policy signing.
    user_policy_builders_[0] = GetUserPolicyBuilder(kTestUsers[0]);
    user_policy_builders_[1] = GetUserPolicyBuilder(kTestUsers[1]);

    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ash::Shell::GetInstance()->
        desktop_background_controller()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // ash::DesktopBackgroundControllerObserver:
  virtual void OnWallpaperDataChanged() OVERRIDE {
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
    base::JSONWriter::Write(policy::test::ConstructExternalDataReference(
        embedded_test_server()->GetURL(std::string("/") + relative_path).spec(),
        image_data).get(),
        &policy);
    return policy;
  }

  // Inject |filename| as wallpaper policy for test user |user_number|.  Set
  // empty |filename| to clear policy.
  void InjectPolicy(int user_number, const std::string& filename) {
    ASSERT_TRUE(user_number == 0 || user_number == 1);
    const std::string user_id = kTestUsers[user_number];
    policy::UserPolicyBuilder* builder =
        user_policy_builders_[user_number].get();
    if (filename != "") {
      builder->payload().
          mutable_wallpaperimage()->set_value(ConstructPolicy(filename));
    } else {
      builder->payload().Clear();
    }
    builder->Build();
    fake_session_manager_client_->set_user_policy(user_id, builder->GetBlob());
    const user_manager::User* user = UserManager::Get()->FindUser(user_id);
    ASSERT_TRUE(user);
    policy::CloudPolicyStore* store = GetStoreForUser(user);
    ASSERT_TRUE(store);
    store->Load();
    ASSERT_EQ(policy::CloudPolicyStore::STATUS_OK, store->status());
    ASSERT_EQ(policy::CloudPolicyValidatorBase::VALIDATION_OK,
              store->validation_status());
  }

  // Obtain WallpaperInfo for |user_number| from WallpaperManager.
  void GetUserWallpaperInfo(int user_number, WallpaperInfo* wallpaper_info) {
    WallpaperManager::Get()->
        GetUserWallpaperInfo(kTestUsers[user_number], wallpaper_info);
  }

  base::FilePath test_data_dir_;
  scoped_ptr<base::RunLoop> run_loop_;
  int wallpaper_change_count_;
  scoped_ptr<policy::UserPolicyBuilder> user_policy_builders_[2];
  FakeDBusThreadManager* fake_dbus_thread_manager_;
  FakeSessionManagerClient* fake_session_manager_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerPolicyTest);
};

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PRE_SetResetClear) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  StartupUtils::MarkOobeCompleted();
}

// Verifies that the wallpaper can be set and re-set through policy and that
// setting policy for a user that is not logged in doesn't affect the current
// user.  Also verifies that after the policy has been cleared, the wallpaper
// reverts to default.
IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, SetResetClear) {
  WallpaperInfo info;
  LoginUser(kTestUsers[0]);
  base::RunLoop().RunUntilIdle();

  // First user: Wait until default wallpaper has been loaded (happens
  // automatically) and store color to recognize it later.
  RunUntilWallpaperChangeCount(1);
  const SkColor original_background_color = GetAverageBackgroundColor();

  // Second user: Set wallpaper policy to blue image.  This should not result in
  // a wallpaper change, which is checked at the very end of this test.
  InjectPolicy(1, kBlueImageFileName);

  // First user: Set wallpaper policy to red image and verify average color.
  InjectPolicy(0, kRedImageFileName);
  RunUntilWallpaperChangeCount(2);
  GetUserWallpaperInfo(0, &info);
  ASSERT_EQ(user_manager::User::POLICY, info.type);
  ASSERT_EQ(kRedImageColor, GetAverageBackgroundColor());

  // First user: Set wallpaper policy to green image and verify average color.
  InjectPolicy(0, kGreenImageFileName);
  RunUntilWallpaperChangeCount(3);
  GetUserWallpaperInfo(0, &info);
  ASSERT_EQ(user_manager::User::POLICY, info.type);
  ASSERT_EQ(kGreenImageColor, GetAverageBackgroundColor());

  // First user: Clear wallpaper policy and verify that the default wallpaper is
  // set again.
  InjectPolicy(0, "");
  RunUntilWallpaperChangeCount(4);
  GetUserWallpaperInfo(0, &info);
  ASSERT_EQ(user_manager::User::DEFAULT, info.type);
  ASSERT_EQ(original_background_color, GetAverageBackgroundColor());

  // Check wallpaper change count to ensure that setting the second user's
  // wallpaper didn't have any effect.
  ASSERT_EQ(4, wallpaper_change_count_);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_PRE_PRE_PRE_WallpaperOnLoginScreen) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_PRE_PRE_WallpaperOnLoginScreen) {
  LoginUser(kTestUsers[0]);

  // Wait until default wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);

  // Set wallpaper policy to red image.
  InjectPolicy(0, kRedImageFileName);

  // Run until wallpaper has changed.
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kRedImageColor, GetAverageBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_PRE_WallpaperOnLoginScreen) {
  LoginUser(kTestUsers[1]);

  // Wait until default wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);

  // Set wallpaper policy to green image.
  InjectPolicy(1, kGreenImageFileName);

  // Run until wallpaper has changed.
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kGreenImageColor, GetAverageBackgroundColor());
}

// Disabled due to flakiness: http://crbug.com/385648.
IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest,
                       DISABLED_WallpaperOnLoginScreen) {
  // Wait for active pod's wallpaper to be loaded.
  RunUntilWallpaperChangeCount(1);
  ASSERT_EQ(kGreenImageColor, GetAverageBackgroundColor());

  // Select the second pod (belonging to user 1).
  ASSERT_TRUE(content::ExecuteScript(
      static_cast<chromeos::LoginDisplayHostImpl*>(
          chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI()->
              web_ui()->GetWebContents(),
      "document.getElementsByClassName('pod')[1].focus();"));
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kRedImageColor, GetAverageBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PRE_PRE_PersistOverLogout) {
  RegisterUser(kTestUsers[0]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PRE_PersistOverLogout) {
  LoginUser(kTestUsers[0]);

  // Wait until default wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);

  // Set wallpaper policy to red image.
  InjectPolicy(0, kRedImageFileName);

  // Run until wallpaper has changed.
  RunUntilWallpaperChangeCount(2);
  ASSERT_EQ(kRedImageColor, GetAverageBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerPolicyTest, PersistOverLogout) {
  LoginUser(kTestUsers[0]);

  // Wait until wallpaper has been loaded.
  RunUntilWallpaperChangeCount(1);
  ASSERT_EQ(kRedImageColor, GetAverageBackgroundColor());
}

}  // namespace chromeos
