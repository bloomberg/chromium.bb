// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_

#include <vector>

class PermissionBubbleDelegate;
class PermissionBubbleManager;

// This class is the platform-independent base class which the
// manager uses to communicate to the UI surface. The UI toolkit
// must set the manager to the view for each tab change caused
// in the window.
class PermissionBubbleView {
 public:
  class Delegate {
    virtual void ToggleAccept(int index, bool new_value) = 0;
    virtual void Accept() = 0;
    virtual void Deny() = 0;
    virtual void Closing() = 0;
  };

  // Set the delegate which will receive UI events forwarded from the bubble.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Used for live updates to the bubble.
  virtual void AddPermissionBubbleDelegate(
      PermissionBubbleDelegate* delegate) = 0;
  virtual void RemovePermissionBubbleDelegate(
      PermissionBubbleDelegate* delegate) = 0;

  virtual void Show(
      const std::vector<PermissionBubbleDelegate*>& delegates,
      const std::vector<bool>& accept_state) = 0;
  virtual void Hide() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_
