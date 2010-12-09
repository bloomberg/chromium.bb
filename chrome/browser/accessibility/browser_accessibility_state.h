// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_H_
#pragma once

#include "base/basictypes.h"

template <typename T> struct DefaultSingletonTraits;

// The BrowserAccessibilityState class is used to determine if Chrome should be
// customized for users with assistive technology, such as screen readers. We
// modify the behavior of certain user interfaces to provide a better experience
// for screen reader users. The way we detect a screen reader program is
// different for each platform.
//
// Screen Reader Detection
// (1) On windows many screen reader detection mechinisms will give false
// positives like relying on the SPI_GETSCREENREADER system parameter. In Chrome
// we attempt to dynamically detect a MSAA client screen reader by calling
// NotifiyWinEvent in WidgetWin with a custom ID and wait to see if the ID
// is requested by a subsequent call to WM_GETOBJECT.
// (2) On mac we detect if VoiceOver is running. This is stored in a preference
// file for Universal Access with the key "voiceOverOnOffKey".
class BrowserAccessibilityState {
 public:
  // Returns the singleton instance.
  static BrowserAccessibilityState* GetInstance();

  ~BrowserAccessibilityState();

  // Called when screen reader client is detected.
  void OnScreenReaderDetected();

  // Returns true if the Chrome browser should be customized for accessibility.
  bool IsAccessibleBrowser();

 private:
  BrowserAccessibilityState();
  friend struct DefaultSingletonTraits<BrowserAccessibilityState>;

  // Set to true when a screen reader client is detected.
  bool screen_reader_active_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityState);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_H_
