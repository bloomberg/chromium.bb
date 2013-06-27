// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/common/pref_names.h"

using extensions::ShellWindowRegistry;

namespace chromeos {

namespace {

// AppWindowWatcher watches for app window and exits the session when the
// last app window is closed.
class AppWindowWatcher : public ShellWindowRegistry::Observer {
 public:
  AppWindowWatcher() : window_registry_(NULL) {}
  virtual ~AppWindowWatcher() {}

  void Init(Profile* profile) {
    DCHECK(!window_registry_);
    window_registry_ = ShellWindowRegistry::Get(profile);
    if (window_registry_)
      window_registry_->AddObserver(this);
  }

 private:
  // extensions::ShellWindowRegistry::Observer overrides:
  virtual void OnShellWindowAdded(ShellWindow* shell_window) OVERRIDE {}
  virtual void OnShellWindowIconChanged(ShellWindow* shell_window) OVERRIDE {}
  virtual void OnShellWindowRemoved(ShellWindow* shell_window) OVERRIDE {
    if (window_registry_->shell_windows().empty()) {
      chrome::AttemptUserExit();
      window_registry_->RemoveObserver(this);
    }
  }

  extensions::ShellWindowRegistry* window_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowWatcher);
};

base::LazyInstance<AppWindowWatcher> app_window_watcher
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitAppSession(Profile* profile, const std::string& app_id) {
  // Binds the session lifetime with app window counts.
  CHECK(app_window_watcher == NULL);
  app_window_watcher.Get().Init(profile);

  // Set the app_id for the current instance of KioskAppUpdateService.
  KioskAppUpdateService* update_service =
      KioskAppUpdateServiceFactory::GetForProfile(profile);
  DCHECK(update_service);
  if (update_service)
    update_service->set_app_id(app_id);

  // If the device is not enterprise managed, set prefs to reboot after update.
  if (!g_browser_process->browser_policy_connector()->IsEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kRebootAfterUpdate, true);
  }
}

}  // namespace chromeos
