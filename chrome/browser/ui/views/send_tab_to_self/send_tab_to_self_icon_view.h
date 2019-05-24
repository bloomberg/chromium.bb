// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace send_tab_to_self {

class SendTabToSelfBubbleController;

// The location bar icon to show the send tab to self bubble where the user can
// choose to share the url to a target device.
class SendTabToSelfIconView : public PageActionIconView {
 public:
  SendTabToSelfIconView(CommandUpdater* command_updater,
                        PageActionIconView::Delegate* delegate);
  ~SendTabToSelfIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  bool Update() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  SendTabToSelfBubbleController* GetController() const;
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  // Whether the animation has been shown.
  bool was_animation_shown_ = false;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfIconView);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ICON_VIEW_H_
