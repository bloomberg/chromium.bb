// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_

#include "content/common/content_export.h"

namespace content {

// The BrowserAccessibilityState class is used to determine if the browser
// should be customized for users with assistive technology, such as screen
// readers.
class CONTENT_EXPORT BrowserAccessibilityState {
 public:
  virtual ~BrowserAccessibilityState() { }

  // Returns the singleton instance.
  static BrowserAccessibilityState* GetInstance();

  // Called when accessibility is enabled manually (via command-line flag).
  virtual void OnAccessibilityEnabledManually() = 0;

  // Called when screen reader client is detected.
  virtual void OnScreenReaderDetected() = 0;

  // Returns true if the browser should be customized for accessibility.
  virtual bool IsAccessibleBrowser() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_ACCESSIBILITY_STATE_H_
