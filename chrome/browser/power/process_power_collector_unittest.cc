// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power/process_power_collector.h"

#include "chrome/browser/apps/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/power/origin_power_map.h"
#include "components/power/origin_power_map_factory.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#endif

using power::OriginPowerMap;
using power::OriginPowerMapFactory;

class BrowserProcessPowerTest : public BrowserWithTestWindowTest {
 public:
  BrowserProcessPowerTest() {}
  virtual ~BrowserProcessPowerTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    collector.reset(new ProcessPowerCollector);

#if defined(OS_CHROMEOS)
    power_manager::PowerSupplyProperties prop;
    prop.set_external_power(power_manager::PowerSupplyProperties::AC);
    prop.set_battery_state(power_manager::PowerSupplyProperties::DISCHARGING);
    prop.set_battery_percent(20.00);
    prop.set_battery_discharge_rate(1);
    collector->PowerChanged(prop);
#endif

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  virtual void TearDown() OVERRIDE {
    collector.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  // Mocks out CPU usage for all processes as |value| percent.
  double ReturnCpuAsConstant(double value, base::ProcessHandle handle) {
    return value;
  }

 protected:
  content::MockRenderProcessHost* GetProcess(Browser* browser) {
    return static_cast<content::MockRenderProcessHost*>(
        browser->tab_strip_model()
            ->GetActiveWebContents()
            ->GetRenderViewHost()
            ->GetProcess());
  }

  scoped_ptr<base::ProcessHandle> MakeProcessHandle(int process_id) {
    scoped_ptr<base::ProcessHandle> proc_handle(new base::ProcessHandle(
#if defined(OS_WIN)
        reinterpret_cast<HANDLE>(process_id))
#else
        process_id)
#endif
                                                );
    return proc_handle.Pass();
  }

  scoped_ptr<ProcessPowerCollector> collector;
  scoped_ptr<TestingProfileManager> profile_manager_;
};

class TestAppWindowContents : public extensions::AppWindowContents {
 public:
  explicit TestAppWindowContents(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  // apps:AppWindowContents
  virtual void Initialize(content::BrowserContext* context,
                          const GURL& url) OVERRIDE {}
  virtual void LoadContents(int32 creator_process_id) OVERRIDE {}
  virtual void NativeWindowChanged(
      extensions::NativeAppWindow* native_app_window) OVERRIDE {}
  virtual void NativeWindowClosed() OVERRIDE {}
  virtual void DispatchWindowShownForTests() const OVERRIDE {}
  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return web_contents_.get();
  }

 private:
  scoped_ptr<content::WebContents> web_contents_;
};

TEST_F(BrowserProcessPowerTest, NoSite) {
  collector->UpdatePowerConsumptionForTesting();
  EXPECT_EQ(0u, collector->metrics_map_for_testing()->size());
}

TEST_F(BrowserProcessPowerTest, OneSite) {
  GURL url("http://www.google.com");
  AddTab(browser(), url);
  collector->UpdatePowerConsumptionForTesting();
  ProcessPowerCollector::ProcessMetricsMap* metrics_map =
      collector->metrics_map_for_testing();
  EXPECT_EQ(1u, metrics_map->size());

  // Create fake process numbers.
  GetProcess(browser())->SetProcessHandle(MakeProcessHandle(1).Pass());

  OriginPowerMap* origin_power_map =
      OriginPowerMapFactory::GetForBrowserContext(profile());
  EXPECT_EQ(0, origin_power_map->GetPowerForOrigin(url));

  collector->set_cpu_usage_callback_for_testing(
      base::Bind(&BrowserProcessPowerTest::ReturnCpuAsConstant,
                 base::Unretained(this),
                 5));
  EXPECT_DOUBLE_EQ(5, collector->UpdatePowerConsumptionForTesting());
  EXPECT_EQ(100, origin_power_map->GetPowerForOrigin(url));
}

TEST_F(BrowserProcessPowerTest, MultipleSites) {
  Browser::CreateParams native_params(profile(),
                                      chrome::HOST_DESKTOP_TYPE_NATIVE);
  GURL url1("http://www.google.com");
  GURL url2("http://www.example.com");
  GURL url3("https://www.google.com");
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));
  AddTab(browser(), url1);
  AddTab(browser2.get(), url2);
  AddTab(browser3.get(), url3);

  // Create fake process numbers.
  GetProcess(browser())->SetProcessHandle(MakeProcessHandle(1).Pass());
  GetProcess(browser2.get())->SetProcessHandle(MakeProcessHandle(2).Pass());
  GetProcess(browser3.get())->SetProcessHandle(MakeProcessHandle(3).Pass());

  collector->UpdatePowerConsumptionForTesting();
  ProcessPowerCollector::ProcessMetricsMap* metrics_map =
      collector->metrics_map_for_testing();
  EXPECT_EQ(3u, metrics_map->size());

  // Since all handlers are uninitialized, this should be 0.
  EXPECT_DOUBLE_EQ(0, collector->UpdatePowerConsumptionForTesting());
  OriginPowerMap* origin_power_map =
      OriginPowerMapFactory::GetForBrowserContext(profile());
  EXPECT_EQ(0, origin_power_map->GetPowerForOrigin(url1));
  EXPECT_EQ(0, origin_power_map->GetPowerForOrigin(url2));
  EXPECT_EQ(0, origin_power_map->GetPowerForOrigin(url3));

  collector->set_cpu_usage_callback_for_testing(
      base::Bind(&BrowserProcessPowerTest::ReturnCpuAsConstant,
                 base::Unretained(this),
                 5));
  EXPECT_DOUBLE_EQ(15, collector->UpdatePowerConsumptionForTesting());
  EXPECT_EQ(33, origin_power_map->GetPowerForOrigin(url1));
  EXPECT_EQ(33, origin_power_map->GetPowerForOrigin(url2));
  EXPECT_EQ(33, origin_power_map->GetPowerForOrigin(url3));

  // Close some tabs and verify that they are removed from the metrics map.
  chrome::CloseTab(browser2.get());
  chrome::CloseTab(browser3.get());

  collector->UpdatePowerConsumptionForTesting();
  EXPECT_EQ(1u, metrics_map->size());
}

TEST_F(BrowserProcessPowerTest, IncognitoDoesntRecordPowerUsage) {
  Browser::CreateParams native_params(profile()->GetOffTheRecordProfile(),
                                      chrome::HOST_DESKTOP_TYPE_NATIVE);
  scoped_ptr<Browser> incognito_browser(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));
  GURL url("http://www.google.com");
  AddTab(browser(), url);

  GURL hidden_url("http://foo.com");
  AddTab(incognito_browser.get(), hidden_url);

  // Create fake process numbers.
  GetProcess(browser())->SetProcessHandle(MakeProcessHandle(1).Pass());
  GetProcess(incognito_browser.get())
      ->SetProcessHandle(MakeProcessHandle(2).Pass());

  EXPECT_DOUBLE_EQ(0, collector->UpdatePowerConsumptionForTesting());
  ProcessPowerCollector::ProcessMetricsMap* metrics_map =
      collector->metrics_map_for_testing();
  EXPECT_EQ(1u, metrics_map->size());

  OriginPowerMap* origin_power_map =
      OriginPowerMapFactory::GetForBrowserContext(profile());
  EXPECT_EQ(0, origin_power_map->GetPowerForOrigin(url));

  collector->set_cpu_usage_callback_for_testing(
      base::Bind(&BrowserProcessPowerTest::ReturnCpuAsConstant,
                 base::Unretained(this),
                 5));
  EXPECT_DOUBLE_EQ(5, collector->UpdatePowerConsumptionForTesting());

  // Verify that the incognito data was not stored.
  EXPECT_EQ(100, origin_power_map->GetPowerForOrigin(url));
  EXPECT_EQ(0, origin_power_map->GetPowerForOrigin(hidden_url));

  chrome::CloseTab(incognito_browser.get());
}

TEST_F(BrowserProcessPowerTest, MultipleProfilesRecordSeparately) {
  scoped_ptr<Profile> other_profile(CreateProfile());
  Browser::CreateParams native_params(other_profile.get(),
                                      chrome::HOST_DESKTOP_TYPE_NATIVE);
  scoped_ptr<Browser> other_user(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));

  GURL url("http://www.google.com");
  AddTab(browser(), url);

  GURL hidden_url("http://foo.com");
  AddTab(other_user.get(), hidden_url);

  // Create fake process numbers.
  GetProcess(browser())->SetProcessHandle(MakeProcessHandle(1).Pass());
  GetProcess(other_user.get())->SetProcessHandle(MakeProcessHandle(2).Pass());

  EXPECT_DOUBLE_EQ(0, collector->UpdatePowerConsumptionForTesting());
  EXPECT_EQ(2u, collector->metrics_map_for_testing()->size());

  collector->set_cpu_usage_callback_for_testing(
      base::Bind(&BrowserProcessPowerTest::ReturnCpuAsConstant,
                 base::Unretained(this),
                 5));
  EXPECT_DOUBLE_EQ(10, collector->UpdatePowerConsumptionForTesting());

  // profile() should have an entry for |url| but not |hidden_url|.
  OriginPowerMap* origin_power_map_first =
      OriginPowerMapFactory::GetForBrowserContext(profile());
  EXPECT_EQ(100, origin_power_map_first->GetPowerForOrigin(url));
  EXPECT_EQ(0, origin_power_map_first->GetPowerForOrigin(hidden_url));

  // |other_profile| should have an entry for |hidden_url| but not |url|.
  OriginPowerMap* origin_power_map_second =
      OriginPowerMapFactory::GetForBrowserContext(other_profile.get());
  EXPECT_EQ(0, origin_power_map_second->GetPowerForOrigin(url));
  EXPECT_EQ(100, origin_power_map_second->GetPowerForOrigin(hidden_url));

  // Clean up
  chrome::CloseTab(other_user.get());
}

TEST_F(BrowserProcessPowerTest, AppsRecordPowerUsage) {
// Install an app (an extension*).
#if defined(OS_WIN)
  base::FilePath extension_path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  base::FilePath extension_path(FILE_PATH_LITERAL("/foo"));
#endif
  base::DictionaryValue manifest;
  manifest.SetString("name", "Fake Name");
  manifest.SetString("version", "1");
  std::string error;
  char kTestAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  scoped_refptr<extensions::Extension> extension(
      extensions::Extension::Create(extension_path,
                                    extensions::Manifest::INTERNAL,
                                    manifest,
                                    extensions::Extension::NO_FLAGS,
                                    kTestAppId,
                                    &error));
  EXPECT_TRUE(extension.get()) << error;

  Profile* current_profile =
      profile_manager_->CreateTestingProfile("Test user");
  GURL url("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  extensions::AppWindow* window = new extensions::AppWindow(
      current_profile, new ChromeAppDelegate(scoped_ptr<ScopedKeepAlive>()),
      extension.get());
  content::WebContents* web_contents(
      content::WebContents::Create(content::WebContents::CreateParams(
          current_profile,
          content::SiteInstance::CreateForURL(current_profile, url))));
  window->SetAppWindowContentsForTesting(
      scoped_ptr<extensions::AppWindowContents>(
          new TestAppWindowContents(web_contents)));
  extensions::AppWindowRegistry* app_registry =
      extensions::AppWindowRegistry::Get(current_profile);
  app_registry->AddAppWindow(window);

  collector->set_cpu_usage_callback_for_testing(
      base::Bind(&BrowserProcessPowerTest::ReturnCpuAsConstant,
                 base::Unretained(this),
                 5));
  collector->UpdatePowerConsumptionForTesting();
  EXPECT_EQ(1u, collector->metrics_map_for_testing()->size());

  // Clear the AppWindowContents before trying to close.
  window->SetAppWindowContentsForTesting(
      scoped_ptr<extensions::AppWindowContents>());
  window->OnNativeClose();
  collector->UpdatePowerConsumptionForTesting();
  EXPECT_EQ(0u, collector->metrics_map_for_testing()->size());
}
