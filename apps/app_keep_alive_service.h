// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_KEEP_ALIVE_SERVICE_H_
#define APPS_APP_KEEP_ALIVE_SERVICE_H_

#include "apps/app_lifetime_monitor.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

namespace apps {

class AppKeepAliveService : public BrowserContextKeyedService,
                            public AppLifetimeMonitor::Observer {
 public:
  AppKeepAliveService(content::BrowserContext* context);
  virtual ~AppKeepAliveService();
  virtual void Shutdown() OVERRIDE;

  // AppLifetimeMonitor::Observer:
  virtual void OnAppStart(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnAppStop(Profile* profile, const std::string& app_id) OVERRIDE;
  virtual void OnAppActivated(Profile* profile,
                              const std::string& app_id) OVERRIDE;
  virtual void OnAppDeactivated(Profile* profile,
                                const std::string& app_id) OVERRIDE;
  virtual void OnChromeTerminating() OVERRIDE;

 private:
  content::BrowserContext* context_;
  std::set<std::string> running_apps_;
  bool shut_down_;

  DISALLOW_COPY_AND_ASSIGN(AppKeepAliveService);
};

}  // namespace apps

#endif  // APPS_APP_KEEP_ALIVE_SERVICE_H_
