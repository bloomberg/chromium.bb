// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"

namespace views {
class View;
}

class PermissionsBubbleDelegateView;

class PermissionBubbleViewViews : public PermissionBubbleView {
 public:
  explicit PermissionBubbleViewViews(views::View* anchor_view);
  virtual ~PermissionBubbleViewViews();

  // PermissionBubbleView:
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;
  virtual void Show(const std::vector<PermissionBubbleRequest*>& requests,
                    const std::vector<bool>& accept_state,
                    bool customization_mode) OVERRIDE;
  virtual bool CanAcceptRequestUpdate() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;

  void Closing();
  void Toggle(int index, bool value);
  void Accept();
  void Deny();
  void SetCustomizationMode();

 private:
  views::View* anchor_view_;
  Delegate* delegate_;
  PermissionsBubbleDelegateView* bubble_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_
