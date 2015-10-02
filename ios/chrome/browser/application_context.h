// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_

#include <string>

#include "base/macros.h"

namespace ios {
class ChromeBrowserStateManager;
}

namespace metrics {
class MetricsService;
}

namespace net {
class URLRequestContextGetter;
}

namespace net_log {
class ChromeNetLog;
}

namespace policy {
class BrowserPolicyConnector;
}

namespace rappor {
class RapporService;
}

class ApplicationContext;
class PrefService;

// Gets the global application context. Cannot return null.
ApplicationContext* GetApplicationContext();

class ApplicationContext {
 public:
  ApplicationContext();
  virtual ~ApplicationContext();

  // Gets the local state associated with this application.
  virtual PrefService* GetLocalState() = 0;

  // Gets the URL request context associated with this application.
  virtual net::URLRequestContextGetter* GetSystemURLRequestContext() = 0;

  // Gets the locale used by the application.
  virtual const std::string& GetApplicationLocale() = 0;

  // Gets the ChromeBrowserStateManager used by this application.
  virtual ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() = 0;

  // Gets the MetricsService used by this application.
  virtual metrics::MetricsService* GetMetricsService() = 0;

  // Gets the policy connector, creating and starting it if necessary.
  virtual policy::BrowserPolicyConnector* GetBrowserPolicyConnector() = 0;

  // Gets the RapporService. May return null.
  virtual rappor::RapporService* GetRapporService() = 0;

  // Gets the ChromeNetLog.
  virtual net_log::ChromeNetLog* GetNetLog() = 0;

 protected:
  // Sets the global ApplicationContext instance.
  static void SetApplicationContext(ApplicationContext* context);

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplicationContext);
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_
