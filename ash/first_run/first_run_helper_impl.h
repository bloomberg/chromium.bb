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
  virtual views::Widget* GetOverlayWidget() OVERRIDE;
  virtual void OpenAppList() OVERRIDE;
  virtual void CloseAppList() OVERRIDE;
  virtual gfx::Rect GetLauncherBounds() OVERRIDE;
  virtual gfx::Rect GetAppListButtonBounds() OVERRIDE;
  virtual gfx::Rect GetAppListBounds() OVERRIDE;
  virtual void OpenTrayBubble() OVERRIDE;
  virtual void CloseTrayBubble() OVERRIDE;
  virtual bool IsTrayBubbleOpened() OVERRIDE;
  virtual gfx::Rect GetTrayBubbleBounds() OVERRIDE;
  virtual gfx::Rect GetHelpButtonBounds() OVERRIDE;

  // Overriden from OverlayEventFilter::Delegate.
  virtual void Cancel() OVERRIDE;
  virtual bool IsCancelingKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;

 private:
  views::Widget* widget_;
  DesktopCleaner cleaner_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperImpl);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_

