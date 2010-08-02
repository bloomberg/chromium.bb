// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_H_
#pragma once

#include <string>

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

// A message from javascript sent via chrome.test.sendMessage().
struct ExtensionTestMessage {
  // The sender's extension id.
  std::string extension_id;

  // The message content.
  std::string content;
};

// This class helps us wait for incoming messages sent from javascript via
// chrome.test.sendMessage(). A sample usage would be:
//
//   ExtensionTestMessageListener listener("foo");
//   ... do some work
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
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
class ExtensionTestMessageListener : public NotificationObserver {
 public:
  // We immediately start listening for |expected_message|.
  explicit ExtensionTestMessageListener(const std::string& expected_message);
  ~ExtensionTestMessageListener();

  // This returns true immediately if we've already gotten the expected
  // message, or waits until it arrives. Returns false if the wait is
  // interrupted and we still haven't gotten the message.
  bool WaitUntilSatisfied();

  // Implements the NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;

  // The message we're expecting.
  std::string expected_message_;

  // Whether we've seen expected_message_ yet.
  bool satisfied_;

  // If we're waiting, then we want to post a quit task when the expected
  // message arrives.
  bool waiting_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_H_
