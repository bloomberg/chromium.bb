// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_HOTWORD_BACKGROUND_ACTIVITY_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_HOTWORD_BACKGROUND_ACTIVITY_DELEGATE_H_

namespace app_list {

// Delegate for HotwordBackgroundActivityMonitor.
class HotwordBackgroundActivityDelegate {
 public:
  virtual ~HotwordBackgroundActivityDelegate() {}

  // Returns the render process ID for the hotword recognizer of the delegate.
  // The value is used to check if the media request comes from its own render
  // process.
  virtual int GetRenderProcessID() = 0;

  // Called when the background hotword recognizer's availablility has changed.
  virtual void OnHotwordBackgroundActivityChanged() = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_HOTWORD_BACKGROUND_ACTIVITY_DELEGATE_H_
