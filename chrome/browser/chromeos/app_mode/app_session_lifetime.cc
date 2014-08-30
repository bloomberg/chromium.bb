// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace chromeos {

namespace {

// AppWindowHandler watches for app window and exits the session when the
// last app window is closed.
class AppWindowHandler : public AppWindowRegistry::Observer {
 public:
  AppWindowHandler() : window_registry_(NULL) {}
  virtual ~AppWindowHandler() {}

  void Init(Profile* profile) {
    DCHECK(!window_registry_);
    window_registry_ = AppWindowRegistry::Get(profile);
    if (window_registry_)
      window_registry_->AddObserver(this);
  }

 private:
  // extensions::AppWindowRegistry::Observer overrides:
  virtual void OnAppWindowRemoved(AppWindow* app_window) OVERRIDE {
    if (window_registry_->app_windows().empty()) {
      if (DemoAppLauncher::IsDemoAppSession(
              user_manager::UserManager::Get()->GetActiveUser()->email())) {
        // If we were in demo mode, we disabled all our network technologies,
        // re-enable them.
        NetworkStateHandler* handler =
            NetworkHandler::Get()->network_state_handler();
        handler->SetTechnologyEnabled(
            NetworkTypePattern::NonVirtual(),
            true,
            chromeos::network_handler::ErrorCallback());
      }
      chrome::AttemptUserExit();
      window_registry_->RemoveObserver(this);
    }
  }

  AppWindowRegistry* window_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowHandler);
};

base::LazyInstance<AppWindowHandler> app_window_handler
    = LAZY_INSTANCE_INITIALIZER;

// BrowserWindowHandler monitors Browser object being created during
// a kiosk session, log info such as URL so that the code path could be
// fixed and closes the just opened browser window.
class BrowserWindowHandler : public chrome::BrowserListObserver {
 public:
  BrowserWindowHandler() {
    BrowserList::AddObserver(this);
  }
  virtual ~BrowserWindowHandler() {
    BrowserList::RemoveObserver(this);
  }

 private:
  void HandleBrowser(Browser* browser) {
    content::WebContents* active_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    std::string url_string =
        active_tab ? active_tab->GetURL().spec() : std::string();
    LOG(WARNING) << "Browser opened in kiosk session"
                 << ", url=" << url_string;

    browser->window()->Close();
  }

  // chrome::BrowserListObserver overrides:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserWindowHandler::HandleBrowser,
                   base::Unretained(this),  // LazyInstance, always valid
                   browser));
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowHandler);
};

base::LazyInstance<BrowserWindowHandler> browser_window_handler
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitAppSession(Profile* profile, const std::string& app_id) {
  // Binds the session lifetime with app window counts.
  CHECK(app_window_handler == NULL);
  app_window_handler.Get().Init(profile);

  CHECK(browser_window_handler == NULL);
  browser_window_handler.Get();

  // For a demo app, we don't need to either setup the update service or
  // the idle app name notification.
  if (DemoAppLauncher::IsDemoAppSession(
          user_manager::UserManager::Get()->GetActiveUser()->email()))
    return;

  // Set the app_id for the current instance of KioskAppUpdateService.
  KioskAppUpdateService* update_service =
      KioskAppUpdateServiceFactory::GetForProfile(profile);
  DCHECK(update_service);
  if (update_service)
    update_service->set_app_id(app_id);

  // Start to monitor external update from usb stick.
  KioskAppManager::Get()->MonitorKioskExternalUpdate();

  // If the device is not enterprise managed, set prefs to reboot after update
  // and create a user security message which shows the user the application
  // name and author after some idle timeout.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kRebootAfterUpdate, true);
    KioskModeIdleAppNameNotification::Initialize();
  }
}

}  // namespace chromeos
