// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class KioskAppUpdateServiceTest : public extensions::PlatformAppBrowserTest {
 public:
  KioskAppUpdateServiceTest() : app_(NULL), update_service_(NULL) {}
  virtual ~KioskAppUpdateServiceTest() {}

  // extensions::PlatformAppBrowserTest overrides:
  virtual void SetUpOnMainThread() OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath& temp_dir = temp_dir_.path();

    const base::TimeDelta uptime = base::TimeDelta::FromHours(1);
    const std::string uptime_seconds =
        base::DoubleToString(uptime.InSecondsF());
    const base::FilePath uptime_file = temp_dir.Append("uptime");
    ASSERT_EQ(static_cast<int>(uptime_seconds.size()),
              base::WriteFile(
                  uptime_file, uptime_seconds.c_str(), uptime_seconds.size()));
    uptime_file_override_.reset(
        new base::ScopedPathOverride(chromeos::FILE_UPTIME, uptime_file));

    app_ = LoadExtension(
        test_data_dir_.AppendASCII("api_test/runtime/on_restart_required"));

    // Fake app mode command line.
    CommandLine* command = CommandLine::ForCurrentProcess();
    command->AppendSwitch(switches::kForceAppMode);
    command->AppendSwitchASCII(switches::kAppId, app_->id());

    update_service_ = KioskAppUpdateServiceFactory::GetForProfile(profile());
    update_service_->set_app_id(app_->id());

    content::RunAllBlockingPoolTasksUntilIdle();
  }

  void FireAppUpdateAvailable() {
    update_service_->OnAppUpdateAvailable(app_);
  }

  void FireUpdatedNeedReboot() {
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
    g_browser_process->platform_part()->automatic_reboot_manager()->
        UpdateStatusChanged(status);
  }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<base::ScopedPathOverride> uptime_file_override_;
  const extensions::Extension* app_;  // Not owned.
  KioskAppUpdateService* update_service_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(KioskAppUpdateServiceTest);
};

IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, AppUpdate) {
  FireAppUpdateAvailable();

  ExtensionTestMessageListener listener("app_update", false);
  listener.WaitUntilSatisfied();
}

IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, OsUpdate) {
  g_browser_process->local_state()->SetBoolean(prefs::kRebootAfterUpdate, true);
  FireUpdatedNeedReboot();

  ExtensionTestMessageListener listener("os_update", false);
  listener.WaitUntilSatisfied();
}

IN_PROC_BROWSER_TEST_F(KioskAppUpdateServiceTest, Periodic) {
  g_browser_process->local_state()->SetInteger(
      prefs::kUptimeLimit, base::TimeDelta::FromMinutes(30).InSeconds());

  ExtensionTestMessageListener listener("periodic", false);
  listener.WaitUntilSatisfied();
}

}  // namespace chromeos
