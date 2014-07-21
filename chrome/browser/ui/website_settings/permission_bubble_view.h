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
    virtual ~Delegate() {}

    virtual void ToggleAccept(int index, bool new_value) = 0;
    virtual void SetCustomizationMode() = 0;
    virtual void Accept() = 0;
    virtual void Deny() = 0;
    virtual void Closing() = 0;
    virtual void SetView(PermissionBubbleView* view) = 0;
  };

  virtual ~PermissionBubbleView() {}

  // Sets the delegate which will receive UI events forwarded from the bubble.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Causes the bubble to show up with the given contents. This method may be
  // called with mostly-identical contents to the existing contents. This can
  // happen, for instance, if a new permission is requested and
  // CanAcceptRequestUpdate() is true.
  // Important: the view must not store any of the request objects it receives
  // in this call.
  virtual void Show(
      const std::vector<PermissionBubbleRequest*>& requests,
      const std::vector<bool>& accept_state,
      bool customization_mode) = 0;

  // Returns true if the view can accept a new Show() command to coalesce
  // requests. Currently the policy is that this should return true if the view
  // is being shown and the mouse is not over the view area (!IsMouseHovered).
  virtual bool CanAcceptRequestUpdate() = 0;

  // Hides the permission bubble.
  virtual void Hide() = 0;

  // Returns true if there is a bubble currently showing.
  virtual bool IsVisible() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_VIEW_H_
