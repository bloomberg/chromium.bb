// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_NET_TEST_AUTOMATION_PROVIDER_H_
#define CHROME_FRAME_TEST_NET_TEST_AUTOMATION_PROVIDER_H_

#include "chrome/browser/automation/automation_provider.h"

class TestAutomationResourceMessageFilter;

// Callback interface for TestAutomationProvider.
class TestAutomationProviderDelegate {
 public:
  virtual void OnInitialTabLoaded() = 0;
};

// A slightly customized version of AutomationProvider.
// We override AutomationProvider to be able to filter received messages
// (see TestAutomationResourceMessageFilter) and know when the initial
// ExternalTab has been loaded.
// In order to intercept UrlRequests and make the URLRequestAutomationJob class
// handle requests from unit tests, we also implement URLRequest::Interceptor.
class TestAutomationProvider
    : public AutomationProvider,
      public URLRequest::Interceptor {
 public:
  explicit TestAutomationProvider(Profile* profile,
      TestAutomationProviderDelegate* delegate);

  virtual ~TestAutomationProvider();
  
  // AutomationProvider overrides.
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual bool Send(IPC::Message* msg);

  // URLRequest::Interceptor.
  virtual URLRequestJob* MaybeIntercept(URLRequest* request);

  // Call to instantiate and initialize a new instance of
  // TestAutomationProvider.
  static TestAutomationProvider* NewAutomationProvider(
      Profile* p,
      const std::string& channel,
      TestAutomationProviderDelegate* delegate);

 protected:
  scoped_refptr<TestAutomationResourceMessageFilter> filter_;
  int tab_handle_;
  TestAutomationProviderDelegate* delegate_;
};

#endif CHROME_FRAME_TEST_NET_TEST_AUTOMATION_PROVIDER_H_
