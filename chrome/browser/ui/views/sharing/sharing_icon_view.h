// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class ThrobAnimation;
}  // namespace gfx

// The location bar icon to show the sharing features bubble.
class SharingIconView : public PageActionIconView {
 public:
  explicit SharingIconView(PageActionIconView::Delegate* delegate);
  ~SharingIconView() override;

  void StartLoadingAnimation();
  void StopLoadingAnimation(base::Optional<int> string_id);

  // views::View:
  void OnThemeChanged() override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  double WidthMultiplier() const override;
  const gfx::VectorIcon& GetVectorIconBadge() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  void UpdateInkDrop(bool activate);
  void UpdateOpacity();
  void UpdateLoaderColor();

  std::unique_ptr<gfx::ThrobAnimation> loading_animation_;
  bool show_error_ = false;
  SkColor loader_color_;

  DISALLOW_COPY_AND_ASSIGN(SharingIconView);
};
#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_ICON_VIEW_H_
