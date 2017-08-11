// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_DELEGATE_H_

#include <memory>

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace system_logs {
class SystemLogsFetcher;
}  // namespace system_logs

namespace extensions {

// Delegate class for embedder-specific chrome.feedbackPrivate behavior.
class FeedbackPrivateDelegate {
 public:
  virtual ~FeedbackPrivateDelegate() = default;

  // Returns a dictionary of localized strings for the feedback component
  // extension.
  // Set |from_crash| to customize strings when the feedback UI was initiated
  // from a "sad tab" crash.
  virtual std::unique_ptr<base::DictionaryValue> GetStrings(
      content::BrowserContext* browser_context,
      bool from_crash) const = 0;

  // Returns a SystemLogsFetcher for responding to a request for system logs.
  virtual system_logs::SystemLogsFetcher* CreateSystemLogsFetcher(
      content::BrowserContext* context) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_DELEGATE_H_
