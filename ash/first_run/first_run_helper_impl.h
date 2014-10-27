// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_

#include "ash/first_run/first_run_helper.h"
#include "ash/first_run/desktop_cleaner.h"
#include "ash/wm/overlay_event_filter.h"
#include "base/compiler_specific.h"

namespace ash {

class Shell;

class FirstRunHelperImpl : public FirstRunHelper,
                           public OverlayEventFilter::Delegate {
 public:
  FirstRunHelperImpl();
  ~FirstRunHelperImpl() override;

  // Overriden from FirstRunHelper.
  views::Widget* GetOverlayWidget() override;
  void OpenAppList() override;
  void CloseAppList() override;
  gfx::Rect GetLauncherBounds() override;
  gfx::Rect GetAppListButtonBounds() override;
  gfx::Rect GetAppListBounds() override;
  void OpenTrayBubble() override;
  void CloseTrayBubble() override;
  bool IsTrayBubbleOpened() override;
  gfx::Rect GetTrayBubbleBounds() override;
  gfx::Rect GetHelpButtonBounds() override;

  // Overriden from OverlayEventFilter::Delegate.
  void Cancel() override;
  bool IsCancelingKeyEvent(ui::KeyEvent* event) override;
  aura::Window* GetWindow() override;

 private:
  views::Widget* widget_;
  DesktopCleaner cleaner_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperImpl);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_

