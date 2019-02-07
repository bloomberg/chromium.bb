// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kTestUrl1[] = "https://test-url-1.com/";
const char kTestInstallUrl1[] = "https://test-url-1.com/install";
const char kTestUrl2[] = "https://test-url-2.com/";

web_app::PendingAppManager::AppInfo GetAppInfoForUrl(const GURL& url) {
  web_app::PendingAppManager::AppInfo info(url,
                                           web_app::LaunchContainer::kWindow,
                                           web_app::InstallSource::kInternal);
  info.override_previous_user_uninstall = true;
  info.bypass_service_worker_check = true;
  info.require_manifest = true;
  return info;
}

class FakeCookieManager : public network::TestCookieManager {
 public:
  FakeCookieManager() = default;
  ~FakeCookieManager() override {
    EXPECT_TRUE(set_canonical_cookie_calls_.empty());
    EXPECT_TRUE(delete_cookies_calls_.empty());
  }

  void InvokePendingSetCanonicalCookieCallback(
      const std::string& expected_cookie_name,
      const std::string& expected_cookie_value,
      bool expected_secure_source,
      bool expected_modify_http_only,
      bool success) {
    ASSERT_FALSE(set_canonical_cookie_calls_.empty());
    auto params = std::move(set_canonical_cookie_calls_.front());
    set_canonical_cookie_calls_.erase(set_canonical_cookie_calls_.begin());

    EXPECT_EQ(expected_cookie_name, std::get<0>(params).Name());
    EXPECT_EQ(expected_cookie_value, std::get<0>(params).Value());
    EXPECT_EQ(expected_secure_source, std::get<1>(params));
    EXPECT_EQ(expected_modify_http_only, std::get<2>(params));

    std::move(std::get<3>(params)).Run(success);
  }

  void InvokePendingDeleteCookiesCallback(
      const GURL& expected_url,
      const std::string& expected_cookie_name,
      bool success) {
    ASSERT_FALSE(delete_cookies_calls_.empty());
    auto params = std::move(delete_cookies_calls_.front());
    delete_cookies_calls_.erase(delete_cookies_calls_.begin());

    EXPECT_EQ(expected_url, params.first->url);
    EXPECT_EQ(expected_cookie_name, params.first->cookie_name);

    std::move(params.second).Run(success);
  }

  // network::mojom::CookieManager
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only,
                          SetCanonicalCookieCallback callback) override {
    set_canonical_cookie_calls_.emplace_back(
        cookie, secure_source, modify_http_only, std::move(callback));
  }

  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {
    delete_cookies_calls_.emplace_back(std::move(filter), std::move(callback));
  }

 private:
  std::vector<
      std::tuple<net::CanonicalCookie, bool, bool, SetCanonicalCookieCallback>>
      set_canonical_cookie_calls_;
  std::vector<
      std::pair<network::mojom::CookieDeletionFilterPtr, DeleteCookiesCallback>>
      delete_cookies_calls_;
};

}  // namespace

class AndroidSmsAppSetupControllerImplTest : public testing::Test {
 protected:
  class TestPwaDelegate : public AndroidSmsAppSetupControllerImpl::PwaDelegate {
   public:
    explicit TestPwaDelegate(FakeCookieManager* fake_cookie_manager)
        : fake_cookie_manager_(fake_cookie_manager) {}
    ~TestPwaDelegate() override = default;

    void SetHasPwa(const GURL& url, bool has_pwa) {
      // If no PWA should exist, erase any existing entry and return.
      if (!has_pwa) {
        url_to_pwa_map_.erase(url);
        return;
      }

      // If a PWA already exists for this URL, there is nothing to do.
      if (base::ContainsKey(url_to_pwa_map_, url))
        return;

      // Create a test Extension and add it to |url_to_pwa_map_|.
      base::FilePath path;
      base::PathService::Get(extensions::DIR_TEST_DATA, &path);
      url_to_pwa_map_[url] = extensions::ExtensionBuilder(url.spec())
                                 .SetPath(path.AppendASCII(url.spec()))
                                 .Build();
    }

    // AndroidSmsAppSetupControllerImpl::PwaDelegate:
    const extensions::Extension* GetPwaForUrl(const GURL& install_url,
                                              Profile* profile) override {
      if (!base::ContainsKey(url_to_pwa_map_, install_url))
        return nullptr;

      return url_to_pwa_map_[install_url].get();
    }

    network::mojom::CookieManager* GetCookieManager(const GURL& app_url,
                                                    Profile* profile) override {
      return fake_cookie_manager_;
    }

   private:
    FakeCookieManager* fake_cookie_manager_;
    base::flat_map<GURL, scoped_refptr<const extensions::Extension>>
        url_to_pwa_map_;
  };

  AndroidSmsAppSetupControllerImplTest()
      : host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(&profile_)) {}

  ~AndroidSmsAppSetupControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    host_content_settings_map_->ClearSettingsForOneType(
        ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    fake_cookie_manager_ = std::make_unique<FakeCookieManager>();
    auto test_pwa_delegate =
        std::make_unique<TestPwaDelegate>(fake_cookie_manager_.get());
    test_pwa_delegate_ = test_pwa_delegate.get();
    test_pending_app_manager_ =
        std::make_unique<web_app::TestPendingAppManager>();
    setup_controller_ = base::WrapUnique(new AndroidSmsAppSetupControllerImpl(
        &profile_, test_pending_app_manager_.get(),
        host_content_settings_map_));

    std::unique_ptr<AndroidSmsAppSetupControllerImpl::PwaDelegate>
        base_delegate(test_pwa_delegate.release());

    static_cast<AndroidSmsAppSetupControllerImpl*>(setup_controller_.get())
        ->SetPwaDelegateForTesting(std::move(base_delegate));
  }

  void CallSetUpApp(const GURL& app_url,
                    const GURL& install_url,
                    size_t num_expected_app_installs) {
    const auto& install_requests =
        test_pending_app_manager_->install_requests();
    size_t num_install_requests_before_call = install_requests.size();

    base::RunLoop run_loop;
    base::HistogramTester histogram_tester;

    setup_controller_->SetUpApp(
        app_url, install_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::OnSetUpAppResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    fake_cookie_manager_->InvokePendingSetCanonicalCookieCallback(
        "default_to_persist" /* expected_cookie_name */,
        "true" /* expected_cookie_value */, true /* expected_secure_source */,
        false /* expected_modify_http_only */, true /* success */);

    fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
        app_url, "cros_migrated_to" /* expected_cookie_name */,
        true /* success */);

    // If the PWA was not already installed at the URL, SetUpApp() should
    // install it.
    if (!test_pwa_delegate_->GetPwaForUrl(install_url, &profile_)) {
      EXPECT_EQ(num_install_requests_before_call + 1u, install_requests.size());
      EXPECT_EQ(GetAppInfoForUrl(install_url), install_requests.back());

      EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
                GetNotificationSetting(app_url));
    }

    if (num_expected_app_installs) {
      histogram_tester.ExpectBucketCount("AndroidSms.PWAInstallationResult",
                                         web_app::InstallResultCode::kSuccess,
                                         num_expected_app_installs);
    }

    run_loop.Run();
    EXPECT_TRUE(*last_set_up_app_result_);
    last_set_up_app_result_.reset();
  }

  void CallDeleteRememberDeviceByDefaultCookie(const GURL& app_url) {
    base::RunLoop run_loop;

    setup_controller_->DeleteRememberDeviceByDefaultCookie(
        app_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::
                           OnDeleteRememberDeviceByDefaultCookieResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
        app_url, "default_to_persist" /* expected_cookie_name */,
        true /* success */);

    run_loop.Run();
    EXPECT_TRUE(*last_delete_cookie_result_);
    last_delete_cookie_result_.reset();
  }

  void CallRemoveApp(const GURL& app_url,
                     const GURL& install_url,
                     const GURL& migrated_to_app_url,
                     size_t num_expected_app_uninstalls) {
    const auto& uninstall_requests =
        test_pending_app_manager_->uninstall_requests();
    size_t num_uninstall_requests_before_call = uninstall_requests.size();

    base::RunLoop run_loop;
    base::HistogramTester histogram_tester;

    setup_controller_->RemoveApp(
        app_url, install_url, migrated_to_app_url,
        base::BindOnce(&AndroidSmsAppSetupControllerImplTest::OnRemoveAppResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    // If the PWA was already installed at the URL, RemoveApp() should uninstall
    // the it.
    if (test_pwa_delegate_->GetPwaForUrl(install_url, &profile_)) {
      EXPECT_EQ(num_uninstall_requests_before_call + 1u,
                uninstall_requests.size());
      EXPECT_EQ(install_url, uninstall_requests.back());

      fake_cookie_manager_->InvokePendingSetCanonicalCookieCallback(
          "cros_migrated_to" /* expected_cookie_name */,
          migrated_to_app_url.GetContent() /* expected_cookie_value */,
          true /* expected_secure_source */,
          false /* expected_modify_http_only */, true /* success */);

      fake_cookie_manager_->InvokePendingDeleteCookiesCallback(
          app_url, "default_to_persist" /* expected_cookie_name */,
          true /* success */);
    }

    if (num_expected_app_uninstalls) {
      histogram_tester.ExpectBucketCount("AndroidSms.PWAUninstallationResult",
                                         true, num_expected_app_uninstalls);
    }

    run_loop.Run();
    EXPECT_TRUE(*last_remove_app_result_);
    last_remove_app_result_.reset();
  }

  TestPwaDelegate* test_pwa_delegate() { return test_pwa_delegate_; }

 private:
  ContentSetting GetNotificationSetting(const GURL& url) {
    std::unique_ptr<base::Value> notification_settings_value =
        host_content_settings_map_->GetWebsiteSetting(
            url, GURL() /* top_level_url */,
            ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
            content_settings::ResourceIdentifier(), nullptr);
    return static_cast<ContentSetting>(notification_settings_value->GetInt());
  }

  void OnSetUpAppResult(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_set_up_app_result_);
    last_set_up_app_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnDeleteRememberDeviceByDefaultCookieResult(
      base::OnceClosure quit_closure,
      bool success) {
    EXPECT_FALSE(last_delete_cookie_result_);
    last_delete_cookie_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnRemoveAppResult(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_remove_app_result_);
    last_remove_app_result_ = success;
    std::move(quit_closure).Run();
  }

  content::TestBrowserThreadBundle thread_bundle_;

  base::Optional<bool> last_set_up_app_result_;
  base::Optional<bool> last_delete_cookie_result_;
  base::Optional<bool> last_remove_app_result_;

  TestingProfile profile_;
  HostContentSettingsMap* host_content_settings_map_;
  std::unique_ptr<FakeCookieManager> fake_cookie_manager_;
  std::unique_ptr<web_app::TestPendingAppManager> test_pending_app_manager_;
  TestPwaDelegate* test_pwa_delegate_;
  std::unique_ptr<AndroidSmsAppSetupController> setup_controller_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppSetupControllerImplTest);
};

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_NoPreviousApp) {
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_AppAlreadyInstalled) {
  // Start with a PWA already installed at the URL.
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1), true);
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               0u /* num_expected_app_installs */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpApp_OtherPwaInstalled) {
  // Start with a PWA already installed at a different URL.
  test_pwa_delegate()->SetHasPwa(GURL(kTestUrl2), true);
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpAppThenDeleteCookie) {
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
  CallDeleteRememberDeviceByDefaultCookie(GURL(kTestUrl1));
}

TEST_F(AndroidSmsAppSetupControllerImplTest, SetUpAppThenRemove) {
  // Install and remove.
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1), true);
  CallRemoveApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                GURL(kTestUrl2) /* migrated_to_app_url */,
                1u /* num_expected_app_uninstalls */);
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1), false);

  // Repeat once more.
  CallSetUpApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
               1u /* num_expected_app_installs */);
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1), true);
  CallRemoveApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                GURL(kTestUrl2) /* migrated_to_app_url */,
                1u /* num_expected_app_uninstalls */);
  test_pwa_delegate()->SetHasPwa(GURL(kTestInstallUrl1), false);
}

TEST_F(AndroidSmsAppSetupControllerImplTest, RemoveApp_NoInstalledApp) {
  // Do not have an installed app before attempting to remove it.
  CallRemoveApp(GURL(kTestUrl1), GURL(kTestInstallUrl1),
                GURL(kTestUrl2) /* migrated_to_app_url */,
                0u /* num_expected_app_uninstalls */);
}

}  // namespace android_sms

}  // namespace chromeos
