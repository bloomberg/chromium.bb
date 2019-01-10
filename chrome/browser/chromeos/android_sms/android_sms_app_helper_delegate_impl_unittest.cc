// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_helper_delegate_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class FakeCookieManager : public network::mojom::CookieManager {
 public:
  const std::vector<std::tuple<net::CanonicalCookie, bool, bool>>&
  set_canonical_cookie_calls() {
    return set_canonical_cookie_calls_;
  }

  const std::vector<network::mojom::CookieDeletionFilterPtr>&
  delete_cookies_calls() {
    return delete_cookies_calls_;
  }

  // network::mojom::CookieManager
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only,
                          SetCanonicalCookieCallback callback) override {
    set_canonical_cookie_calls_.push_back(
        std::make_tuple(cookie, secure_source, modify_http_only));
    std::move(callback).Run(true);
  }

  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {
    delete_cookies_calls_.push_back(std::move(filter));
    std::move(callback).Run(1);
  }

  void GetAllCookies(GetAllCookiesCallback callback) override {}
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback) override {}
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override {}
  void AddCookieChangeListener(
      const GURL& url,
      const std::string& name,
      network::mojom::CookieChangeListenerPtr listener) override {}
  void AddGlobalChangeListener(
      network::mojom::CookieChangeListenerPtr notification_pointer) override {}
  void CloneInterface(
      network::mojom::CookieManagerRequest new_interface) override {}
  void FlushCookieStore(FlushCookieStoreCallback callback) override {}
  void SetContentSettings(
      const std::vector<::ContentSettingPatternSource>& settings) override {}
  void SetForceKeepSessionState() override {}
  void BlockThirdPartyCookies(bool block) override {}

 private:
  std::vector<std::tuple<net::CanonicalCookie, bool, bool>>
      set_canonical_cookie_calls_;
  std::vector<network::mojom::CookieDeletionFilterPtr> delete_cookies_calls_;
};
}  // namespace

namespace chromeos {

namespace android_sms {

class AndroidSmsAppHelperDelegateImplTest : public testing::Test {
 protected:
  class TestPwaDelegate : public AndroidSmsAppHelperDelegateImpl::PwaDelegate {
   public:
    explicit TestPwaDelegate(FakeCookieManager* fake_cookie_manager)
        : fake_cookie_manager_(fake_cookie_manager) {}
    ~TestPwaDelegate() override = default;

    void SetHasPwa(GURL gurl, bool has_pwa) {
      // If no PWA should exist, erase any existing entry and return.
      if (!has_pwa) {
        gurl_to_pwa_map_.erase(gurl);
        return;
      }

      // If a PWA already exists for this URL, there is nothing to do.
      if (base::ContainsKey(gurl_to_pwa_map_, gurl))
        return;

      // Create a test Extension and add it to |gurl_to_pwa_map_|.
      base::FilePath path;
      base::PathService::Get(extensions::DIR_TEST_DATA, &path);
      gurl_to_pwa_map_[gurl] = extensions::ExtensionBuilder(gurl.spec())
                                   .SetPath(path.AppendASCII(gurl.spec()))
                                   .Build();
    }

    const std::vector<AppLaunchParams>& opened_app_launch_params() const {
      return opened_app_launch_params_;
    }

    // AndroidSmsAppHelperDelegateImpl::PwaDelegate:
    const extensions::Extension* GetPwaForUrl(Profile* profile,
                                              GURL gurl) override {
      if (!base::ContainsKey(gurl_to_pwa_map_, gurl))
        return nullptr;

      return gurl_to_pwa_map_[gurl].get();
    }

    content::WebContents* OpenApp(const AppLaunchParams& params) override {
      opened_app_launch_params_.push_back(params);
      return nullptr;
    }

    network::mojom::CookieManager* GetCookieManager(Profile* profile) override {
      return fake_cookie_manager_;
    }

   private:
    FakeCookieManager* fake_cookie_manager_;

    base::flat_map<GURL, scoped_refptr<const extensions::Extension>>
        gurl_to_pwa_map_;
    std::vector<AppLaunchParams> opened_app_launch_params_;
  };

  class TestPendingAppManager : public web_app::TestPendingAppManager {
   public:
    explicit TestPendingAppManager(TestPwaDelegate* test_pwa_delegate)
        : web_app::TestPendingAppManager(),
          test_pwa_delegate_(test_pwa_delegate) {}
    ~TestPendingAppManager() override = default;

    // PendingAppManager:
    void Install(AppInfo app_to_install,
                 OnceInstallCallback callback) override {
      test_pwa_delegate_->SetHasPwa(app_to_install.url, true);
      web_app::TestPendingAppManager::Install(app_to_install,
                                              std::move(callback));
    }

   private:
    TestPwaDelegate* test_pwa_delegate_;

    DISALLOW_COPY_AND_ASSIGN(TestPendingAppManager);
  };

  AndroidSmsAppHelperDelegateImplTest()
      : host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(&profile_)) {}

  ~AndroidSmsAppHelperDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    host_content_settings_map_->ClearSettingsForOneType(
        ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    fake_cookie_manager_ = std::make_unique<FakeCookieManager>();
    auto test_pwa_delegate =
        std::make_unique<TestPwaDelegate>(fake_cookie_manager_.get());
    test_pwa_delegate_ = test_pwa_delegate.get();
    test_pending_app_manager_ =
        std::make_unique<TestPendingAppManager>(test_pwa_delegate_);
    android_sms_app_helper_delegate_ =
        base::WrapUnique(new AndroidSmsAppHelperDelegateImpl(
            &profile_, test_pending_app_manager_.get(),
            host_content_settings_map_));

    std::unique_ptr<AndroidSmsAppHelperDelegateImpl::PwaDelegate> base_delegate(
        test_pwa_delegate.release());

    static_cast<AndroidSmsAppHelperDelegateImpl*>(
        android_sms_app_helper_delegate_.get())
        ->SetPwaDelegateForTesting(std::move(base_delegate));
  }

  TestPendingAppManager* test_pending_app_manager() {
    return test_pending_app_manager_.get();
  }

  FakeCookieManager* fake_cookie_manager() {
    return fake_cookie_manager_.get();
  }

  void TearDownApp() {
    android_sms_app_helper_delegate_->TearDownAndroidSmsApp();
  }

  ContentSetting GetNotificationSetting() {
    std::unique_ptr<base::Value> notification_settings_value =
        host_content_settings_map_->GetWebsiteSetting(
            GetAndroidMessagesURL(), GURL() /* top_level_url */,
            ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
            content_settings::ResourceIdentifier(), nullptr);
    return static_cast<ContentSetting>(notification_settings_value->GetInt());
  }

  void TestSetUpMessagesApp(bool also_launch_app) {
    base::HistogramTester histogram_tester;
    EXPECT_NE(ContentSetting::CONTENT_SETTING_ALLOW, GetNotificationSetting());

    if (also_launch_app)
      android_sms_app_helper_delegate_->SetUpAndLaunchAndroidSmsApp();
    else
      android_sms_app_helper_delegate_->SetUpAndroidSmsApp();

    // Construct AppInfo which is expected to be requested for install.
    web_app::PendingAppManager::AppInfo info(GetAndroidMessagesURL(),
                                             web_app::LaunchContainer::kWindow,
                                             web_app::InstallSource::kInternal);
    info.override_previous_user_uninstall = true;
    info.bypass_service_worker_check = true;
    info.require_manifest = true;

    // Verify that the installation was requested.
    EXPECT_EQ(std::vector<web_app::PendingAppManager::AppInfo>{info},
              test_pending_app_manager()->install_requests());
    EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW, GetNotificationSetting());
    histogram_tester.ExpectBucketCount("AndroidSms.PWAInstallationResult",
                                       web_app::InstallResultCode::kSuccess, 1);

    // Verify that the default_to_persist cookie is set.
    ASSERT_EQ(1u, fake_cookie_manager()->set_canonical_cookie_calls().size());
    std::tuple<net::CanonicalCookie, bool, bool> set_cookie_call =
        fake_cookie_manager()->set_canonical_cookie_calls()[0];
    EXPECT_EQ("default_to_persist", std::get<0>(set_cookie_call).Name());
    EXPECT_EQ("true", std::get<0>(set_cookie_call).Value());
    EXPECT_TRUE(std::get<1>(set_cookie_call));
    EXPECT_FALSE(std::get<2>(set_cookie_call));

    if (also_launch_app) {
      EXPECT_EQ(
          test_pwa_delegate_->GetPwaForUrl(&profile_, GetAndroidMessagesURL())
              ->id(),
          test_pwa_delegate_->opened_app_launch_params().back().extension_id);
    }
  }

  void VerifyCookieDeletedForUrl(GURL gurl) {
    ASSERT_EQ(1u, fake_cookie_manager()->delete_cookies_calls().size());
    const network::mojom::CookieDeletionFilterPtr& delete_filter =
        fake_cookie_manager()->delete_cookies_calls()[0];
    EXPECT_EQ(gurl, delete_filter->url);
    EXPECT_EQ("default_to_persist", delete_filter->cookie_name);
  }

  TestPwaDelegate* test_pwa_delegate() { return test_pwa_delegate_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  HostContentSettingsMap* host_content_settings_map_;
  std::unique_ptr<FakeCookieManager> fake_cookie_manager_;
  std::unique_ptr<TestPendingAppManager> test_pending_app_manager_;
  TestPwaDelegate* test_pwa_delegate_;
  std::unique_ptr<multidevice_setup::AndroidSmsAppHelperDelegate>
      android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImplTest);
};

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestSetUpMessagesApp_NoOldApp) {
  TestSetUpMessagesApp(false /* also_launch_app */);

  // No app should have been uninstalled in this process.
  EXPECT_EQ(0u, test_pending_app_manager()->uninstall_requests().size());
}

TEST_F(AndroidSmsAppHelperDelegateImplTest,
       TestSetUpMessagesApp_UninstallsOldApp) {
  base::HistogramTester histogram_tester;

  // Simulate a PWA having already been installed at the old URL.
  test_pwa_delegate()->SetHasPwa(GetAndroidMessagesURLOld(),
                                 true /* has_pwa */);

  TestSetUpMessagesApp(false /* also_launch_app */);

  // The old app should have been uninstalled.
  ASSERT_EQ(1u, test_pending_app_manager()->uninstall_requests().size());
  EXPECT_EQ(GetAndroidMessagesURLOld(),
            test_pending_app_manager()->uninstall_requests()[0]);
  histogram_tester.ExpectBucketCount("AndroidSms.PWAUninstallationResult",
                                     true /* success */, 1);

  // The old app's cookie should have been deleted.
  VerifyCookieDeletedForUrl(GetAndroidMessagesURLOld());
}

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestTearDownMessagesApp) {
  TearDownApp();
  VerifyCookieDeletedForUrl(GetAndroidMessagesURL());
}

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallAndLaunchMessagesApp) {
  TestSetUpMessagesApp(true /* also_launch_app */);
}

}  // namespace android_sms

}  // namespace chromeos
