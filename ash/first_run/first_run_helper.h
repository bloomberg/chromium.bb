// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

#include "ash/ash_export.h"
#include "ash/first_run/desktop_cleaner.h"
#include "ash/wm/overlay_event_filter.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

// Interface used by first-run tutorial to manipulate and retrieve information
// about shell elements.
// All returned coordinates are in screen coordinate system.
class ASH_EXPORT FirstRunHelper : public OverlayEventFilter::Delegate {
 public:
  class Observer {
   public:
    // Called when first-run UI was cancelled.
    virtual void OnCancelled() = 0;
    virtual ~Observer() {}
  };

 public:
  FirstRunHelper();
  virtual ~FirstRunHelper();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns widget to place tutorial UI into it.
  views::Widget* GetOverlayWidget();

  // Returns bounds of application list button.
  gfx::Rect GetAppListButtonBounds();

  // Opens and closes system tray bubble.
  void OpenTrayBubble();
  void CloseTrayBubble();

  // Returns |true| iff system tray bubble is opened now.
  bool IsTrayBubbleOpened();

  // Returns bounds of system tray bubble. You must open bubble before calling
  // this method.
  gfx::Rect GetTrayBubbleBounds();

  // Returns bounds of help app button from system tray buble. You must open
  // bubble before calling this method.
  gfx::Rect GetHelpButtonBounds();

  // OverlayEventFilter::Delegate:
  void Cancel() override;
  bool IsCancelingKeyEvent(ui::KeyEvent* event) override;
  aura::Window* GetWindow() override;

 private:
  base::ObserverList<Observer> observers_;

  // The first run dialog window.
  views::Widget* widget_;

  DesktopCleaner cleaner_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelper);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_H_
