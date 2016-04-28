// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_export.h"
#include "ash/pointer_watcher_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/events/event_handler.h"

namespace ash {

// Support for PointerWatchers in non-mus ash, implemented with a pre-target
// EventHandler on the Shell.
class ASH_EXPORT PointerWatcherDelegateAura : public PointerWatcherDelegate,
                                              public ui::EventHandler {
 public:
  PointerWatcherDelegateAura();
  ~PointerWatcherDelegateAura() override;

  // PointerWatcherDelegate:
  void AddPointerWatcher(views::PointerWatcher* watcher) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  // Must be empty on destruction.
  base::ObserverList<views::PointerWatcher, true> pointer_watchers_;

  DISALLOW_COPY_AND_ASSIGN(PointerWatcherDelegateAura);
};

}  // namespace ash
