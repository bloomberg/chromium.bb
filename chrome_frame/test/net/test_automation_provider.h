// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_NET_TEST_AUTOMATION_PROVIDER_H_
#define CHROME_FRAME_TEST_NET_TEST_AUTOMATION_PROVIDER_H_

#include <string>
#include "chrome/browser/automation/automation_provider.h"

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

class TestAutomationResourceMessageFilter;

// Callback interface for TestAutomationProvider.
class TestAutomationProviderDelegate {
 public:
  virtual void OnInitialTabLoaded() = 0;
  virtual void OnProviderDestroyed() = 0;
};

// A slightly customized version of AutomationProvider.
// We override AutomationProvider to be able to filter received messages
// (see TestAutomationResourceMessageFilter) and know when the initial
// ExternalTab has been loaded.
// In order to intercept UrlRequests and make the URLRequestAutomationJob class
// handle requests from unit tests, we register a protocol factory for
// http/https.
class TestAutomationProvider
    : public AutomationProvider {
 public:
  TestAutomationProvider(Profile* profile,
                         TestAutomationProviderDelegate* delegate);

  virtual ~TestAutomationProvider();

  // AutomationProvider overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual bool Send(IPC::Message* msg);

  // Protocol factory for handling http/https requests over automation.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

  // Call to instantiate and initialize a new instance of
  // TestAutomationProvider.
  static TestAutomationProvider* NewAutomationProvider(
      Profile* p,
      const std::string& channel,
      TestAutomationProviderDelegate* delegate);

 protected:
  virtual std::string GetProtocolVersion();

  int tab_handle_;
  TestAutomationProviderDelegate* delegate_;

  static TestAutomationProvider* g_provider_instance_;
};

#endif  // CHROME_FRAME_TEST_NET_TEST_AUTOMATION_PROVIDER_H_
