// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_
#define IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "ios/chrome/browser/application_context.h"

namespace base {
class CommandLine;
}

class ApplicationContextImpl : public ApplicationContext {
 public:
  ApplicationContextImpl(const base::CommandLine& command_line);
  ~ApplicationContextImpl() override;

  // Sets the locale used by the application.
  void SetApplicationLocale(const std::string& locale);

 private:
  // ApplicationContext implementation.
  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  const std::string& GetApplicationLocale() override;
  ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() override;
  metrics::MetricsService* GetMetricsService() override;
  policy::BrowserPolicyConnector* GetBrowserPolicyConnector() override;
  rappor::RapporService* GetRapporService() override;
  net_log::ChromeNetLog* GetNetLog() override;

  base::ThreadChecker thread_checker_;
  scoped_ptr<net_log::ChromeNetLog> net_log_;
  std::string application_locale_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationContextImpl);
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_
