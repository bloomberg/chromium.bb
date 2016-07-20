// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_export.h"
#include "ash/common/pointer_down_watcher_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/events/event_handler.h"

namespace gfx {
class Point;
}

namespace ui {
class LocatedEvent;
}

namespace views {
class Widget;
}

namespace ash {

// Support for PointerDownWatchers in non-mus ash, implemented with a pre-target
// EventHandler on the Shell.
class ASH_EXPORT PointerDownWatcherDelegateAura
    : public PointerDownWatcherDelegate,
      public ui::EventHandler {
 public:
  PointerDownWatcherDelegateAura();
  ~PointerDownWatcherDelegateAura() override;

  // PointerDownWatcherDelegate:
  void AddPointerDownWatcher(views::PointerDownWatcher* watcher) override;
  void RemovePointerDownWatcher(views::PointerDownWatcher* watcher) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  gfx::Point GetLocationInScreen(const ui::LocatedEvent& event) const;
  views::Widget* GetTargetWidget(const ui::LocatedEvent& event) const;

  // Must be empty on destruction.
  base::ObserverList<views::PointerDownWatcher, true> pointer_down_watchers_;

  DISALLOW_COPY_AND_ASSIGN(PointerDownWatcherDelegateAura);
};

}  // namespace ash
