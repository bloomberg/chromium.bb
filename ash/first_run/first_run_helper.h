// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/observer_list.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

// Interface used by first-run tutorial to manipulate and retreive information
// about shell elements.
// All returned coordinates are in screen coordinate system.
class ASH_EXPORT FirstRunHelper {
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
  virtual views::Widget* GetOverlayWidget() = 0;

  // Opens and closes app list.
  virtual void OpenAppList() = 0;
  virtual void CloseAppList() = 0;

  // Returns bounding rectangle of launcher elements.
  virtual gfx::Rect GetLauncherBounds() = 0;

  // Returns bounds of application list button.
  virtual gfx::Rect GetAppListButtonBounds() = 0;

  // Returns bounds of application list. You must open application list before
  // calling this method.
  virtual gfx::Rect GetAppListBounds() = 0;

  // Opens and closes system tray bubble.
  virtual void OpenTrayBubble() = 0;
  virtual void CloseTrayBubble() = 0;

  // Returns |true| iff system tray bubble is opened now.
  virtual bool IsTrayBubbleOpened() = 0;

  // Returns bounds of system tray bubble. You must open bubble before calling
  // this method.
  virtual gfx::Rect GetTrayBubbleBounds() = 0;

  // Returns bounds of help app button from system tray buble. You must open
  // bubble before calling this method.
  virtual gfx::Rect GetHelpButtonBounds() = 0;

 protected:
  ObserverList<Observer>& observers() { return observers_; }

 private:
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelper);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

