// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_

#include <vector>

class PermissionBubbleRequest;
class PermissionBubbleManager;

// This class is the platform-independent interface through which the permission
// bubble managers (which are one per tab) communicate to the UI surface.
// When the visible tab changes, the UI code must provide an object of this type
// to the manager for the visible tab.
class PermissionBubbleView {
 public:
  // The delegate will receive events caused by user action which need to
  // be persisted in the per-tab UI state.
  class Delegate {
   public:
    virtual void ToggleAccept(int index, bool new_value) = 0;
    virtual void SetCustomizationMode() = 0;
    virtual void Accept() = 0;
    virtual void Deny() = 0;
    virtual void Closing() = 0;
  };

  // Sets the delegate which will receive UI events forwarded from the bubble.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Causes the bubble to show up with the given contents.
  virtual void Show(
      const std::vector<PermissionBubbleRequest*>& requests,
      const std::vector<bool>& accept_state,
      bool custommization_mode) = 0;

  // Hides the permission bubble.
  virtual void Hide() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_
