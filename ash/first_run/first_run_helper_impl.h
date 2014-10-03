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
  virtual ~FirstRunHelperImpl();

  // Overriden from FirstRunHelper.
  virtual views::Widget* GetOverlayWidget() override;
  virtual void OpenAppList() override;
  virtual void CloseAppList() override;
  virtual gfx::Rect GetLauncherBounds() override;
  virtual gfx::Rect GetAppListButtonBounds() override;
  virtual gfx::Rect GetAppListBounds() override;
  virtual void OpenTrayBubble() override;
  virtual void CloseTrayBubble() override;
  virtual bool IsTrayBubbleOpened() override;
  virtual gfx::Rect GetTrayBubbleBounds() override;
  virtual gfx::Rect GetHelpButtonBounds() override;

  // Overriden from OverlayEventFilter::Delegate.
  virtual void Cancel() override;
  virtual bool IsCancelingKeyEvent(ui::KeyEvent* event) override;
  virtual aura::Window* GetWindow() override;

 private:
  views::Widget* widget_;
  DesktopCleaner cleaner_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperImpl);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_

