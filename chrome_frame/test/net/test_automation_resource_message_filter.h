// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_NET_TEST_AUTOMATION_RESOURCE_MESSAGE_FILTER_H_
#define CHROME_FRAME_TEST_NET_TEST_AUTOMATION_RESOURCE_MESSAGE_FILTER_H_

#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/automation/url_request_automation_job.h"

// Performs the same duties as AutomationResourceMessageFilter but with one
// difference.  Instead of being tied to an IPC channel running on Chrome's
// IO thread, this instance runs on the unit test's IO thread (all URL request
// tests have their own IO loop) and is tied to an instance of
// AutomationProvider (TestAutomationProvider to be exact).
//
// Messages from the AutomationProvider that are destined to request objects
// owned by this class are marshaled over to the request's IO thread instead
// of using the thread the messages are received on.  This way we allow the
// URL request tests to run sequentially as they were written while still
// allowing the automation layer to work as it normally does (i.e. process
// its messages on the receiving thread).
class TestAutomationResourceMessageFilter
    : public AutomationResourceMessageFilter {
 public:
  explicit TestAutomationResourceMessageFilter(AutomationProvider* automation);

  virtual bool Send(IPC::Message* message);

  static void OnRequestMessage(URLRequestAutomationJob* job,
                               IPC::Message* msg);

  virtual bool OnMessageReceived(const IPC::Message& message);

  // Add request to the list of outstanding requests.
  virtual bool RegisterRequest(URLRequestAutomationJob* job);
  // Remove request from the list of outstanding requests.
  virtual void UnRegisterRequest(URLRequestAutomationJob* job);

 protected:
  AutomationProvider* automation_;
  // declare the request map.
  struct RequestJob {
    MessageLoop* loop_;
    scoped_refptr<URLRequestAutomationJob> job_;
  };
  typedef std::map<int, RequestJob> RequestMap;
  RequestMap requests_;
};

#endif  // CHROME_FRAME_TEST_NET_TEST_AUTOMATION_RESOURCE_MESSAGE_FILTER_H_
