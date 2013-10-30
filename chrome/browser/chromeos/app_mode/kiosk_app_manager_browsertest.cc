// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/test/test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {

namespace {

const char kWebstoreDomain[] = "cws.com";

// Helper KioskAppManager::GetConsumerKioskModeStatusCallback implementation.
void ConsumerKioskModeStatusCheck(
    KioskAppManager::ConsumerKioskModeStatus* out_status,
    const base::Closure& runner_quit_task,
    KioskAppManager::ConsumerKioskModeStatus in_status) {
  LOG(INFO) << "ConsumerKioskModeStatus = " << in_status;
  *out_status = in_status;
  runner_quit_task.Run();
}

// Helper KioskAppManager::EnableKioskModeCallback implementation.
void ConsumerKioskModeLockCheck(
    bool* out_locked,
    const base::Closure& runner_quit_task,
    bool in_locked) {
  LOG(INFO) << "kioks locked  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

// Helper EnterpriseInstallAttributes::LockResultCallback implementation.
void OnEnterpriseDeviceLock(
    policy::EnterpriseInstallAttributes::LockResult* out_locked,
    const base::Closure& runner_quit_task,
    policy::EnterpriseInstallAttributes::LockResult in_locked) {
  LOG(INFO) << "Enterprise lock  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

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
    base::MessageLoop::current()->Run();
  }

  bool loaded() const { return loaded_; }

 private:
  // KioskAppManagerObserver overrides:
  virtual void OnKioskAppDataChanged(const std::string& app_id) OVERRIDE {
    loaded_ = true;
    base::MessageLoop::current()->Quit();
  }
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) OVERRIDE {
    loaded_ = false;
    base::MessageLoop::current()->Quit();
  }

  KioskAppManager* manager_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(AppDataLoadWaiter);
};

}  // namespace

class KioskAppManagerTest : public InProcessBrowserTest {
 public:
  KioskAppManagerTest() {}
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

  std::string GetAppIds() const {
    KioskAppManager::Apps apps;
    manager()->GetApps(&apps);

    std::string str;
    for (size_t i = 0; i < apps.size(); ++i) {
      if (i > 0)
        str += ',';
      str += apps[i].app_id;
    }

    return str;
  }

  KioskAppManager* manager() const { return KioskAppManager::Get(); }

  // Locks device for enterprise.
  policy::EnterpriseInstallAttributes::LockResult LockDeviceForEnterprise() {
    scoped_ptr<policy::EnterpriseInstallAttributes::LockResult> lock_result(
        new policy::EnterpriseInstallAttributes::LockResult(
            policy::EnterpriseInstallAttributes::LOCK_NOT_READY));
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    g_browser_process->browser_policy_connector()->GetInstallAttributes()->
        LockDevice(
            "user@domain.com",
            policy::DEVICE_MODE_ENTERPRISE,
            "device-id",
            base::Bind(&OnEnterpriseDeviceLock,
                       lock_result.get(),
                       runner->QuitClosure()));
    runner->Run();
    return *lock_result.get();
  }

 private:
  std::string test_gallery_url_;
  base::ShadowingAtExitManager exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManagerTest);
};

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, Basic) {
  // Add a couple of apps. Use "fake_app_x" that do not have data on the test
  // server to avoid pending data loads that could be lingering on tear down and
  // cause DCHECK failure in utility_process_host_impl.cc.
  manager()->AddApp("fake_app_1");
  manager()->AddApp("fake_app_2");
  EXPECT_EQ("fake_app_1,fake_app_2", GetAppIds());

  // Set an auto launch app.
  manager()->SetAutoLaunchApp("fake_app_1");
  EXPECT_EQ("fake_app_1", manager()->GetAutoLaunchApp());

  // Clear the auto launch app.
  manager()->SetAutoLaunchApp("");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());

  // Set another auto launch app.
  manager()->SetAutoLaunchApp("fake_app_2");
  EXPECT_EQ("fake_app_2", manager()->GetAutoLaunchApp());

  // Check auto launch permissions.
  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());
  manager()->SetEnableAutoLaunch(true);
  EXPECT_TRUE(manager()->IsAutoLaunchEnabled());

  // Remove the auto launch app.
  manager()->RemoveApp("fake_app_2");
  EXPECT_EQ("fake_app_1", GetAppIds());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Add the just removed auto launch app again and it should no longer be
  // the auto launch app.
  manager()->AddApp("fake_app_2");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  manager()->RemoveApp("fake_app_2");
  EXPECT_EQ("fake_app_1", GetAppIds());

  // Set a none exist app as auto launch.
  manager()->SetAutoLaunchApp("none_exist_app");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());

  // Add an existing app again.
  manager()->AddApp("fake_app_1");
  EXPECT_EQ("fake_app_1", GetAppIds());
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

  // Make the app appear in device settings.
  base::ListValue device_local_accounts;
  scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->SetStringWithoutPathExpansion(
      kAccountsPrefDeviceLocalAccountsKeyId,
      "app_1_id");
  entry->SetIntegerWithoutPathExpansion(
      kAccountsPrefDeviceLocalAccountsKeyType,
      policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  entry->SetStringWithoutPathExpansion(
      kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
      "app_1");
  device_local_accounts.Append(entry.release());
  CrosSettings::Get()->Set(kAccountsPrefDeviceLocalAccounts,
                           device_local_accounts);

  AppDataLoadWaiter waiter(manager());
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  KioskAppManager::Apps apps;
  manager()->GetApps(&apps);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ("app_1", apps[0].app_id);
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
  EXPECT_EQ("app_1", apps[0].app_id);
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
      AppendASCII(apps[0].app_id).AddExtension(".png");
  EXPECT_EQ(expected_icon_path.value(), icon_path_string);
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, EnableConsumerKiosk) {
  scoped_ptr<KioskAppManager::ConsumerKioskModeStatus> status(
      new KioskAppManager::ConsumerKioskModeStatus(
          KioskAppManager::CONSUMER_KIOSK_MODE_DISABLED));
  scoped_ptr<bool> locked(new bool(false));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(*status.get(), KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE);

  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  manager()->EnableConsumerModeKiosk(
      base::Bind(&ConsumerKioskModeLockCheck,
                 locked.get(),
                 runner2->QuitClosure()));
  runner2->Run();
  EXPECT_TRUE(*locked.get());

  scoped_refptr<content::MessageLoopRunner> runner3 =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner3->QuitClosure()));
  runner3->Run();
  EXPECT_EQ(*status.get(), KioskAppManager::CONSUMER_KIOSK_MODE_ENABLED);
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest,
                       PreventEnableConsumerKioskForEnterprise) {
  // First, lock the device as enterprise.
  EXPECT_EQ(LockDeviceForEnterprise(),
            policy::EnterpriseInstallAttributes::LOCK_SUCCESS);

  scoped_ptr<KioskAppManager::ConsumerKioskModeStatus> status(
      new KioskAppManager::ConsumerKioskModeStatus(
          KioskAppManager::CONSUMER_KIOSK_MODE_DISABLED));
  scoped_ptr<bool> locked(new bool(true));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(*status.get(), KioskAppManager::CONSUMER_KIOSK_MODE_DISABLED);

  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  manager()->EnableConsumerModeKiosk(
      base::Bind(&ConsumerKioskModeLockCheck,
                 locked.get(),
                 runner2->QuitClosure()));
  runner2->Run();
  EXPECT_FALSE(*locked.get());

  scoped_refptr<content::MessageLoopRunner> runner3 =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner3->QuitClosure()));
  runner3->Run();
  EXPECT_EQ(*status.get(), KioskAppManager::CONSUMER_KIOSK_MODE_DISABLED);
}

}  // namespace chromeos
