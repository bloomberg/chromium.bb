// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "ui/gfx/screen.h"

// A timer based implementation of PanelMouseWatcher.  Currently used for Gtk
// and Mac panels implementations.
class PanelMouseWatcherTimer : public PanelMouseWatcher {
 public:
  PanelMouseWatcherTimer();
  ~PanelMouseWatcherTimer() override;

 private:
  void Start() override;
  void Stop() override;
  bool IsActive() const override;
  gfx::Point GetMousePosition() const override;

  // Specifies the rate at which we want to sample the mouse position.
  static const int kMousePollingIntervalMs = 250;

  // Timer callback function.
  void DoWork();
  friend class base::RepeatingTimer;

  // Timer used to track mouse movements. Some OSes do not provide an easy way
  // of tracking mouse movements across applications.  So we use a timer to
  // accomplish the same.  This could also be more efficient as you end up
  // getting a lot of notifications when tracking mouse movements.
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(PanelMouseWatcherTimer);
};

// static
PanelMouseWatcher* PanelMouseWatcher::Create() {
  return new PanelMouseWatcherTimer();
}

PanelMouseWatcherTimer::PanelMouseWatcherTimer() {
}

PanelMouseWatcherTimer::~PanelMouseWatcherTimer() {
  DCHECK(!IsActive());
}

void PanelMouseWatcherTimer::Start() {
  DCHECK(!IsActive());
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kMousePollingIntervalMs),
               this, &PanelMouseWatcherTimer::DoWork);
}

void PanelMouseWatcherTimer::Stop() {
  DCHECK(IsActive());
  timer_.Stop();
}

bool PanelMouseWatcherTimer::IsActive() const {
  return timer_.IsRunning();
}

gfx::Point PanelMouseWatcherTimer::GetMousePosition() const {
  return gfx::Screen::GetScreen()->GetCursorScreenPoint();
}

void PanelMouseWatcherTimer::DoWork() {
  NotifyMouseMovement(GetMousePosition());
}
