// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_keep_alive_service.h"

#include "apps/app_lifetime_monitor.h"
#include "apps/app_lifetime_monitor_factory.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {

AppKeepAliveService::AppKeepAliveService(content::BrowserContext* context)
    : context_(context), shut_down_(false) {
  AppLifetimeMonitor* app_lifetime_monitor =
      AppLifetimeMonitorFactory::GetForProfile(static_cast<Profile*>(context));
  app_lifetime_monitor->AddObserver(this);
}

AppKeepAliveService::~AppKeepAliveService() {}

void AppKeepAliveService::Shutdown() {
  AppLifetimeMonitor* app_lifetime_monitor =
      AppLifetimeMonitorFactory::GetForProfile(static_cast<Profile*>(context_));
  app_lifetime_monitor->RemoveObserver(this);
  OnChromeTerminating();
}

void AppKeepAliveService::OnAppStart(Profile* profile,
                                     const std::string& app_id) {
  if (profile != context_ || shut_down_)
    return;

  if (running_apps_.insert(app_id).second)
    chrome::StartKeepAlive();
}

void AppKeepAliveService::OnAppStop(Profile* profile,
                                    const std::string& app_id) {
  if (profile != context_)
    return;

  if (running_apps_.erase(app_id))
    chrome::EndKeepAlive();
}

void AppKeepAliveService::OnAppActivated(Profile* profile,
                                         const std::string& app_id) {}

void AppKeepAliveService::OnAppDeactivated(Profile* profile,
                                           const std::string& app_id) {}

void AppKeepAliveService::OnChromeTerminating() {
  shut_down_ = true;
  size_t keep_alives = running_apps_.size();
  running_apps_.clear();

  // In some tests, the message loop isn't running during shutdown and ending
  // the last keep alive in that case CHECKs.
  if (!base::MessageLoop::current() ||
      base::MessageLoop::current()->is_running()) {
    while (keep_alives--)
      chrome::EndKeepAlive();
  }
}

}  // namespace apps
