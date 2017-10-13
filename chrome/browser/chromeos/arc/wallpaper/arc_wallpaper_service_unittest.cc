// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/fake_wallpaper_instance.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SuccessDecodeRequestSender
    : public arc::ArcWallpaperService::DecodeRequestSender {
 public:
  ~SuccessDecodeRequestSender() override = default;
  void SendDecodeRequest(ImageDecoder::ImageRequest* request,
                         const std::vector<uint8_t>& data) override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(256 /* width */, 256 /* height */);
    bitmap.eraseColor(SK_ColorRED);
    request->OnImageDecoded(bitmap);
  }
};

class FailureDecodeRequestSender
    : public arc::ArcWallpaperService::DecodeRequestSender {
 public:
  ~FailureDecodeRequestSender() override = default;
  void SendDecodeRequest(ImageDecoder::ImageRequest* request,
                         const std::vector<uint8_t>& data) override {
    request->OnDecodeImageFailed();
  }
};

class ArcWallpaperServiceTest : public ash::AshTestBase {
 public:
  ArcWallpaperServiceTest()
      : user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(user_manager_) {}
  ~ArcWallpaperServiceTest() override = default;

  void SetUp() override {
    disable_provide_local_state();
    AshTestBase::SetUp();

    // Prefs
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    pref_service_.registry()->RegisterDictionaryPref(
        ash::prefs::kWallpaperColors);
    pref_service_.registry()->RegisterDictionaryPref(
        wallpaper::kUsersWallpaperInfo);

    // Ash prefs
    auto pref_service = std::make_unique<TestingPrefServiceSimple>();
    ash::Shell::RegisterLocalStatePrefs(pref_service->registry());
    pref_service->registry()->SetDefaultForeignPrefValue(
        ash::prefs::kWallpaperColors, std::make_unique<base::DictionaryValue>(),
        0);
    ash::ShellTestApi().OnLocalStatePrefServiceInitialized(
        std::move(pref_service));

    // User
    user_manager_->AddUser(user_manager::StubAccountId());
    user_manager_->LoginUser(user_manager::StubAccountId());
    ASSERT_TRUE(user_manager_->GetPrimaryUser());

    // Wallpaper maanger
    chromeos::WallpaperManager::Initialize();

    // Arc services
    wallpaper_instance_ = std::make_unique<arc::FakeWallpaperInstance>();
    arc_service_manager_.set_browser_context(&testing_profile_);
    arc_service_manager_.arc_bridge_service()->wallpaper()->SetInstance(
        wallpaper_instance_.get());
    service_ =
        arc::ArcWallpaperService::GetForBrowserContext(&testing_profile_);
    ASSERT_TRUE(service_);

    // Salt
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03};
    chromeos::SystemSaltGetter::Get()->SetRawSaltForTesting(salt);
  }

  void TearDown() override {
    chromeos::WallpaperManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    AshTestBase::TearDown();
  }

 protected:
  arc::ArcWallpaperService* service_ = nullptr;
  std::unique_ptr<arc::FakeWallpaperInstance> wallpaper_instance_ = nullptr;

 private:
  chromeos::FakeChromeUserManager* const user_manager_ = nullptr;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  arc::ArcServiceManager arc_service_manager_;
  // testing_profile_ needs to be deleted before arc_service_manager_.
  TestingProfile testing_profile_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperServiceTest);
};

}  // namespace

TEST_F(ArcWallpaperServiceTest, SetDefaultWallpaper) {
  service_->SetDefaultWallpaper();
  RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(-1, wallpaper_instance_->changed_ids()[0]);
}

TEST_F(ArcWallpaperServiceTest, SetAndGetWallpaper) {
  service_->SetDecodeRequestSenderForTesting(
      std::make_unique<SuccessDecodeRequestSender>());
  std::vector<uint8_t> bytes;
  service_->SetWallpaper(bytes, 10 /* ID */);
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(1u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(10, wallpaper_instance_->changed_ids()[0]);

  base::RunLoop run_loop;
  service_->GetWallpaper(
      base::BindOnce([](std::vector<uint8_t>* out,
                        const std::vector<uint8_t>& bytes) { *out = bytes; },
                     &bytes));
  content::RunAllTasksUntilIdle();
  ASSERT_NE(0u, bytes.size());
}

TEST_F(ArcWallpaperServiceTest, SetWallpaperFailure) {
  service_->SetDecodeRequestSenderForTesting(
      std::make_unique<FailureDecodeRequestSender>());
  std::vector<uint8_t> bytes;
  service_->SetWallpaper(bytes, 10 /* ID */);
  content::RunAllTasksUntilIdle();

  // For failure case, ArcWallpaperService reports that wallpaper is changed to
  // requested wallpaper (ID=10), then reports that the wallpaper is changed
  // back to the previous wallpaper immediately.
  ASSERT_EQ(2u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(10, wallpaper_instance_->changed_ids()[0]);
  EXPECT_EQ(-1, wallpaper_instance_->changed_ids()[1]);
}
