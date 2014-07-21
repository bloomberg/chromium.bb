// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents.h"

#ifdef __OBJC__
@class PermissionBubbleController;
#else
class PermissionBubbleController;
#endif

class PermissionBubbleCocoa : public PermissionBubbleView {
 public:
  explicit PermissionBubbleCocoa(NSWindow* parent_window);
  virtual ~PermissionBubbleCocoa();

  // PermissionBubbleView interface.
  virtual void Show(const std::vector<PermissionBubbleRequest*>& requests,
                    const std::vector<bool>& accept_state,
                    bool customization_mode) OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;
  virtual bool CanAcceptRequestUpdate() OVERRIDE;

  // Called when |bubbleController_| is closing.
  void OnBubbleClosing();

  // Returns the point, in screen coordinates, to which the bubble's arrow
  // should point.
  NSPoint GetAnchorPoint();

  // Returns the NSWindow containing the bubble.
  NSWindow* window();

private:
  NSWindow* parent_window_;  // Weak.
  Delegate* delegate_;  // Weak.

  // Cocoa-side UI controller for the bubble.  Weak, as it will close itself.
  PermissionBubbleController* bubbleController_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_PERMISSION_BUBBLE_COCOA_H_
