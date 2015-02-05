// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"

namespace views {
class View;
}

class PermissionsBubbleDelegateView;

class PermissionBubbleViewViews : public PermissionBubbleView {
 public:
  PermissionBubbleViewViews(views::View* anchor_view,
                            const std::string& languages);
  ~PermissionBubbleViewViews() override;

  // PermissionBubbleView:
  void SetDelegate(Delegate* delegate) override;
  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  bool CanAcceptRequestUpdate() override;
  void Hide() override;
  bool IsVisible() override;

  void Closing();
  void Toggle(int index, bool value);
  void Accept();
  void Deny();

 private:
  views::View* anchor_view_;
  Delegate* delegate_;
  PermissionsBubbleDelegateView* bubble_delegate_;
  const std::string languages_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSIONS_BUBBLE_VIEW_H_
