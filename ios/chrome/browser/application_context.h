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

namespace network_time {
class NetworkTimeTracker;
}

namespace policy {
class BrowserPolicyConnector;
class PolicyService;
}

namespace rappor {
class RapporService;
}

namespace variations {
class VariationsService;
}

class ApplicationContext;
class PrefService;

// Gets the global application context. Cannot return null.
ApplicationContext* GetApplicationContext();

class ApplicationContext {
 public:
  ApplicationContext();
  virtual ~ApplicationContext();

  // Invoked when application enters foreground. Cancels the effect of
  // OnAppEnterBackground(), in particular removes the boolean preference
  // indicating that the ChromeBrowserStates have been shutdown.
  virtual void OnAppEnterForeground() = 0;

  // Invoked when application enters background. Saves any state that must be
  // saved before shutdown can continue.
  virtual void OnAppEnterBackground() = 0;

  // Returns whether the last complete shutdown was clean (i.e. happened while
  // the application was backgrounded).
  virtual bool WasLastShutdownClean() = 0;

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

  // Gets the VariationsService used by this application.
  virtual variations::VariationsService* GetVariationsService() = 0;

  // Gets the policy connector, creating and starting it if necessary.
  virtual policy::BrowserPolicyConnector* GetBrowserPolicyConnector() = 0;

  // Gets the policy service.
  virtual policy::PolicyService* GetPolicyService() = 0;

  // Gets the RapporService. May return null.
  virtual rappor::RapporService* GetRapporService() = 0;

  // Gets the ChromeNetLog.
  virtual net_log::ChromeNetLog* GetNetLog() = 0;

  // Gets the NetworkTimeTracker.
  virtual network_time::NetworkTimeTracker* GetNetworkTimeTracker() = 0;

 protected:
  // Sets the global ApplicationContext instance.
  static void SetApplicationContext(ApplicationContext* context);

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplicationContext);
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_
