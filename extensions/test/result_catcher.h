// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_RESULT_CATCHER_H_
#define EXTENSIONS_TEST_RESULT_CATCHER_H_

#include <deque>
#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Helper class that observes tests failing or passing. Observation starts
// when the class is constructed. Get the next result by calling
// GetNextResult() and message() if GetNextResult() return false. If there
// are no results, this method will pump the UI message loop until one is
// received.
class ResultCatcher : public content::NotificationObserver {
 public:
  ResultCatcher();
  virtual ~ResultCatcher();

  // Pumps the UI loop until a notification is received that an API test
  // succeeded or failed. Returns true if the test succeeded, false otherwise.
  bool GetNextResult();

  void RestrictToBrowserContext(content::BrowserContext* context) {
    browser_context_restriction_ = context;
  }

  const std::string& message() { return message_; }

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  // A sequential list of pass/fail notifications from the test extension(s).
  std::deque<bool> results_;

  // If it failed, what was the error message?
  std::deque<std::string> messages_;
  std::string message_;

  // If non-NULL, we will listen to events from this BrowserContext only.
  content::BrowserContext* browser_context_restriction_;

  // True if we're in a nested message loop waiting for results from
  // the extension.
  bool waiting_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_RESULT_CATCHER_H_
