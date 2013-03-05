// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {
class TestSendMessageFunction;
}

// This class helps us wait for incoming messages sent from javascript via
// chrome.test.sendMessage(). A sample usage would be:
//
//   ExtensionTestMessageListener listener("foo");
//   ... do some work
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//
// It is also possible to have the extension wait for our reply. This is
// useful for coordinating multiple pages/processes and having them wait on
// each other. Example:
//
//   ExtensionTestMessageListener listener1("foo1");
//   ExtensionTestMessageListener listener2("foo2");
//   ASSERT_TRUE(listener1.WaitUntilSatisfied());
//   ASSERT_TRUE(listener2.WaitUntilSatisfied());
//   ... do some work
//   listener1.Reply("foo2 is ready");
//   listener2.Reply("foo1 is ready");
//
// TODO(asargent) - In the future we may want to add the ability to listen for
// multiple messages, and/or to wait for "any" message and then retrieve the
// contents of that message. We may also want to specify an extension id as
// satisfaction criteria in addition to message content.
//
// Note that when using it in browser tests, you need to make sure it gets
// destructed *before* the browser gets torn down. Two common patterns are to
// either make it a local variable inside your test body, or if it's a member
// variable of a ExtensionBrowserTest subclass, override the
// InProcessBrowserTest::CleanUpOnMainThread() method and clean it up there.
class ExtensionTestMessageListener : public content::NotificationObserver {
 public:
  // We immediately start listening for |expected_message|.
  ExtensionTestMessageListener(const std::string& expected_message,
                               bool will_reply);
  virtual ~ExtensionTestMessageListener();

  void AlsoListenForFailureMessage(const std::string& failure_message) {
    failure_message_ = failure_message;
  }

  // This returns true immediately if we've already gotten the expected
  // message, or waits until it arrives. Returns false if the wait is
  // interrupted and we still haven't gotten the message.
  bool WaitUntilSatisfied();

  // Send the given message as a reply. It is only valid to call this after
  // WaitUntilSatisfied has returned true, and if will_reply is true.
  void Reply(const std::string& message);

  // Convenience method that formats int as a string and sends it.
  void Reply(int message);

  // Implements the content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  bool was_satisfied() const { return satisfied_; }

 private:
  content::NotificationRegistrar registrar_;

  // The message we're expecting.
  std::string expected_message_;

  // Whether we've seen expected_message_ yet.
  bool satisfied_;

  // If we're waiting, then we want to post a quit task when the expected
  // message arrives.
  bool waiting_;

  // If true, we expect the calling code to manually send a reply. Otherwise,
  // we send an automatic empty reply to the extension.
  bool will_reply_;

  // The message that signals failure.
  std::string failure_message_;

  // If we received a message that was the failure message.
  bool failed_;

  // The function we need to reply to.
  extensions::TestSendMessageFunction* function_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_H_
