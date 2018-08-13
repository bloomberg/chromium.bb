// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

class AssistantController;
class AssistantMainView;
class AssistantMiniView;
class AssistantWebView;

class AssistantContainerView : public views::BubbleDialogDelegateView,
                               public AssistantUiModelObserver,
                               public gfx::AnimationDelegate {
 public:
  explicit AssistantContainerView(AssistantController* assistant_controller);
  ~AssistantContainerView() override;

  // views::BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void PreferredSizeChanged() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void Init() override;
  void RequestFocus() override;

  // AssistantUiModelObserver:
  void OnUiModeChanged(AssistantUiMode ui_mode) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  void SetAnchor();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  AssistantMainView* assistant_main_view_;  // Owned by view hierarchy.
  AssistantMiniView* assistant_mini_view_;  // Owned by view hierarchy.
  AssistantWebView* assistant_web_view_;    // Owned by view hierarchy.

  std::unique_ptr<gfx::SlideAnimation> resize_animation_;
  gfx::SizeF resize_start_;
  gfx::SizeF resize_end_;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
