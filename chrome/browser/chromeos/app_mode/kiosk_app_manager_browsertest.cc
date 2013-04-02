// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {

namespace {

const char kWebstoreDomain[] = "cws.com";

class TestKioskAppManagerObserver : public KioskAppManagerObserver {
 public:
  explicit TestKioskAppManagerObserver(KioskAppManager* manager)
      : manager_(manager),
        data_changed_count_(0),
        load_failure_count_(0) {
    manager_->AddObserver(this);
  }
  virtual ~TestKioskAppManagerObserver() {
    manager_->RemoveObserver(this);
  }

  void Reset() {
    data_changed_count_ = 0;
    load_failure_count_ = 0;
  }

  int data_changed_count() const { return data_changed_count_; }
  int load_failure_count() const { return load_failure_count_; }

 private:
  // KioskAppManagerObserver overrides:
  virtual void OnKioskAppDataChanged(const std::string& app_id) OVERRIDE {
    ++data_changed_count_;
  }
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) OVERRIDE {
    ++load_failure_count_;
  }

  KioskAppManager* manager_;
  int data_changed_count_;
  int load_failure_count_;

  DISALLOW_COPY_AND_ASSIGN(TestKioskAppManagerObserver);
};

class AppDataLoadWaiter : public KioskAppManagerObserver {
 public:
  explicit AppDataLoadWaiter(KioskAppManager* manager)
      : manager_(manager),
        loaded_(false) {
    manager_->AddObserver(this);
  }
  virtual ~AppDataLoadWaiter() {
    manager_->RemoveObserver(this);
  }

  void Wait() {
    MessageLoop::current()->Run();
  }

  bool loaded() const { return loaded_; }

 private:
  // KioskAppManagerObserver overrides:
  virtual void OnKioskAppDataChanged(const std::string& app_id) OVERRIDE {
    loaded_ = true;
    MessageLoop::current()->Quit();
  }
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) OVERRIDE {
    loaded_ = false;
    MessageLoop::current()->Quit();
  }

  KioskAppManager* manager_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(AppDataLoadWaiter);
};

}  // namespace

class KioskAppManagerTest : public InProcessBrowserTest {
 public:
  KioskAppManagerTest()
      : manager_(NULL) {}
  virtual ~KioskAppManagerTest() {}

  // InProcessBrowserTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We start the test server now instead of in
    // SetUpInProcessBrowserTestFixture so that we can get its port number.
    ASSERT_TRUE(test_server()->Start());

    net::HostPortPair host_port = test_server()->host_port_pair();
    test_gallery_url_ = base::StringPrintf(
        "http://%s:%d/files/chromeos/app_mode/webstore",
        kWebstoreDomain, host_port.port());

    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL, test_gallery_url_);
  }
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    host_resolver()->AddRule(kWebstoreDomain, "127.0.0.1");
  }
  virtual void SetUpOnMainThread() OVERRIDE {
    manager_.reset(new KioskAppManager);
  }
  virtual void CleanUpOnMainThread() OVERRIDE {
    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    manager_->CleanUp();
  }

  std::string GetAppIds() const {
    KioskAppManager::Apps apps;
    manager_->GetApps(&apps);

    std::string str;
    for (size_t i = 0; i < apps.size(); ++i) {
      if (i > 0)
        str += ',';
      str += apps[i].id;
    }

    return str;
  }

  KioskAppManager* manager() { return manager_.get(); }

 private:
  scoped_ptr<KioskAppManager> manager_;
  std::string test_gallery_url_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManagerTest);
};

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, Basic) {
  // Add a couple of apps.
  manager()->AddApp("app_1");
  manager()->AddApp("app_2");
  EXPECT_EQ("app_1,app_2", GetAppIds());

  // Set an auto launch app.
  manager()->SetAutoLaunchApp("app_1");
  EXPECT_EQ("app_1", manager()->GetAutoLaunchApp());

  // Clear the auto launch app.
  manager()->SetAutoLaunchApp("");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Set another auto launch app.
  manager()->SetAutoLaunchApp("app_2");
  EXPECT_EQ("app_2", manager()->GetAutoLaunchApp());

  // Remove the auto launch app.
  manager()->RemoveApp("app_2");
  EXPECT_EQ("app_1", GetAppIds());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Set a none exist app as auto launch.
  TestKioskAppManagerObserver observer(manager());
  manager()->SetAutoLaunchApp("none_exist_app");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Add an exist app again.
  observer.Reset();
  manager()->AddApp("app_1");
  EXPECT_EQ("app_1", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, LoadCached) {
  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath data_dir = test_dir.AppendASCII("chromeos/app_mode/");

  scoped_ptr<base::DictionaryValue> apps_dict(new base::DictionaryValue);
  apps_dict->SetString("app_1.name", "App1 Name");
  std::string icon_path =
      base::StringPrintf("%s/red16x16.png", data_dir.value().c_str());
  apps_dict->SetString("app_1.icon", icon_path);

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->Set(KioskAppManager::kKeyApps, apps_dict.release());

  // Triggers reload prefs.
  manager()->RemoveApp("dummy");

  AppDataLoadWaiter waiter(manager());
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  KioskAppManager::Apps apps;
  manager()->GetApps(&apps);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ("app_1", apps[0].id);
  EXPECT_EQ("App1 Name", apps[0].name);
  EXPECT_EQ(gfx::Size(16, 16), apps[0].icon.size());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, BadApp) {
  manager()->AddApp("unknown_app");

  TestKioskAppManagerObserver observer(manager());

  AppDataLoadWaiter waiter(manager());
  waiter.Wait();
  EXPECT_FALSE(waiter.loaded());

  EXPECT_EQ("", GetAppIds());
  EXPECT_EQ(1, observer.load_failure_count());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, GoodApp) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/app_1
  manager()->AddApp("app_1");

  AppDataLoadWaiter waiter(manager());
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  // Check data is correct.
  KioskAppManager::Apps apps;
  manager()->GetApps(&apps);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ("app_1", apps[0].id);
  EXPECT_EQ("Name of App 1", apps[0].name);
  EXPECT_EQ(gfx::Size(16, 16), apps[0].icon.size());

  // Check data is cached in local state.
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

  std::string name;
  EXPECT_TRUE(dict->GetString("apps.app_1.name", &name));
  EXPECT_EQ(apps[0].name, name);

  std::string icon_path_string;
  EXPECT_TRUE(dict->GetString("apps.app_1.icon", &icon_path_string));

  base::FilePath expected_icon_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &expected_icon_path));
  expected_icon_path = expected_icon_path.
      AppendASCII(KioskAppManager::kIconCacheDir).
      AppendASCII(apps[0].id).AddExtension(".png");
  EXPECT_EQ(expected_icon_path.value(), icon_path_string);
}

}  // namespace chromeos
