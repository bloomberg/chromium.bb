// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/wm/window_state.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

using apps::ShellWindowRegistry;

namespace chromeos {

namespace {

// AppWindowHandler watches for app window and exits the session when the
// last app window is closed. It also initializes the kiosk app window so
// that it receives all function keys as a temp solution before underlying
// http:://crbug.com/166928 is fixed..
class AppWindowHandler : public ShellWindowRegistry::Observer {
 public:
  AppWindowHandler() : window_registry_(NULL) {}
  virtual ~AppWindowHandler() {}

  void Init(Profile* profile) {
    DCHECK(!window_registry_);
    window_registry_ = ShellWindowRegistry::Get(profile);
    if (window_registry_)
      window_registry_->AddObserver(this);
  }

 private:
  // apps::ShellWindowRegistry::Observer overrides:
  virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) OVERRIDE {
    // Set flags to allow kiosk app to receive all function keys.
    // TODO(xiyuan): Remove this after http:://crbug.com/166928.
    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(shell_window->GetNativeWindow());
    window_state->set_top_row_keys_are_function_keys(true);
  }
  virtual void OnShellWindowIconChanged(apps::ShellWindow* shell_window)
    OVERRIDE {}
  virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window) OVERRIDE {
    if (window_registry_->shell_windows().empty()) {
      chrome::AttemptUserExit();
      window_registry_->RemoveObserver(this);
    }
  }

  apps::ShellWindowRegistry* window_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowHandler);
};

base::LazyInstance<AppWindowHandler> app_window_handler
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitAppSession(Profile* profile, const std::string& app_id) {
  // Binds the session lifetime with app window counts.
  CHECK(app_window_handler == NULL);
  app_window_handler.Get().Init(profile);

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
