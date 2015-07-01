// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class View;
}

class Browser;
class PermissionsBubbleDelegateView;

class PermissionBubbleViewViews : public PermissionBubbleView {
 public:
  explicit PermissionBubbleViewViews(Browser* browser);
  ~PermissionBubbleViewViews() override;

  // PermissionBubbleView:
  void SetDelegate(Delegate* delegate) override;
  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  bool CanAcceptRequestUpdate() override;
  void Hide() override;
  bool IsVisible() override;
  void UpdateAnchorPosition() override;
  gfx::NativeWindow GetNativeWindow() override;

  void Closing();
  void Toggle(int index, bool value);
  void Accept();
  void Deny();

 private:
  views::View* GetAnchorView();
  views::BubbleBorder::Arrow GetAnchorArrow();

  Browser* browser_;
  Delegate* delegate_;
  PermissionsBubbleDelegateView* bubble_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_
