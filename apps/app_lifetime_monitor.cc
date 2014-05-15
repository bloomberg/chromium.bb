// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_lifetime_monitor.h"

#include "apps/app_window.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/extension.h"

namespace apps {

using extensions::Extension;
using extensions::ExtensionHost;

AppLifetimeMonitor::AppLifetimeMonitor(Profile* profile)
    : profile_(profile) {
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
      content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Factory::GetForBrowserContext(profile_,
                                                       false /* create */);
  DCHECK(app_window_registry);
  app_window_registry->AddObserver(this);
}

AppLifetimeMonitor::~AppLifetimeMonitor() {}

void AppLifetimeMonitor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppLifetimeMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppLifetimeMonitor::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      const Extension* extension = host->extension();
      if (!extension || !extension->is_platform_app())
        return;

      NotifyAppStart(extension->id());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      const Extension* extension = host->extension();
      if (!extension || !extension->is_platform_app())
        return;

      NotifyAppStop(extension->id());
      break;
    }

    case chrome::NOTIFICATION_APP_TERMINATING: {
      NotifyChromeTerminating();
      break;
    }
  }
}

void AppLifetimeMonitor::OnAppWindowRemoved(AppWindow* app_window) {
  if (!HasVisibleAppWindows(app_window))
    NotifyAppDeactivated(app_window->extension_id());
}

void AppLifetimeMonitor::OnAppWindowHidden(AppWindow* app_window) {
  if (!HasVisibleAppWindows(app_window))
    NotifyAppDeactivated(app_window->extension_id());
}

void AppLifetimeMonitor::OnAppWindowShown(AppWindow* app_window) {
  if (app_window->window_type() != AppWindow::WINDOW_TYPE_DEFAULT)
    return;

  if (HasVisibleAppWindows(app_window))
    NotifyAppActivated(app_window->extension_id());
}

void AppLifetimeMonitor::Shutdown() {
  AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Factory::GetForBrowserContext(profile_,
                                                       false /* create */);
  if (app_window_registry)
    app_window_registry->RemoveObserver(this);
}

bool AppLifetimeMonitor::HasVisibleAppWindows(AppWindow* app_window) const {
  AppWindowRegistry::AppWindowList windows =
      AppWindowRegistry::Get(app_window->browser_context())
          ->GetAppWindowsForApp(app_window->extension_id());

  for (AppWindowRegistry::AppWindowList::const_iterator i = windows.begin();
       i != windows.end();
       ++i) {
    if (!(*i)->is_hidden())
      return true;
  }
  return false;
}

void AppLifetimeMonitor::NotifyAppStart(const std::string& app_id) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAppStart(profile_, app_id));
}

void AppLifetimeMonitor::NotifyAppActivated(const std::string& app_id) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAppActivated(profile_, app_id));
}

void AppLifetimeMonitor::NotifyAppDeactivated(const std::string& app_id) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAppDeactivated(profile_, app_id));
}

void AppLifetimeMonitor::NotifyAppStop(const std::string& app_id) {
  FOR_EACH_OBSERVER(Observer, observers_, OnAppStop(profile_, app_id));
}

void AppLifetimeMonitor::NotifyChromeTerminating() {
  FOR_EACH_OBSERVER(Observer, observers_, OnChromeTerminating());
}

}  // namespace apps
