// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_mouse_watcher.h"

#include "chrome/browser/ui/panels/panel_mouse_watcher_observer.h"
#include "ui/gfx/point.h"

PanelMouseWatcher::PanelMouseWatcher() {
}

PanelMouseWatcher::~PanelMouseWatcher() {
}

void PanelMouseWatcher::AddObserver(PanelMouseWatcherObserver* observer) {
  bool already_started = observers_.might_have_observers();
  observers_.AddObserver(observer);
  if (!already_started)
    Start();
}

void PanelMouseWatcher::RemoveObserver(PanelMouseWatcherObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
  if (!observers_.might_have_observers())
    Stop();
}

void PanelMouseWatcher::NotifyMouseMovement(const gfx::Point& mouse_position) {
  FOR_EACH_OBSERVER(PanelMouseWatcherObserver, observers_,
                    OnMouseMove(mouse_position));
}
