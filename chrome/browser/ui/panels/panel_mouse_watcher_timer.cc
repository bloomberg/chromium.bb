// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "ui/gfx/screen.h"

// A timer based implementation of PanelMouseWatcher.  Currently used for Gtk
// and Mac panels implementations.
class PanelMouseWatcherTimer : public PanelMouseWatcher {
 public:
  PanelMouseWatcherTimer();
  virtual ~PanelMouseWatcherTimer();

 private:
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual gfx::Point GetMousePosition() const OVERRIDE;

  // Specifies the rate at which we want to sample the mouse position.
  static const int kMousePollingIntervalMs = 250;

  // Timer callback function.
  void DoWork();
  friend class base::RepeatingTimer<PanelMouseWatcherTimer>;

  // Timer used to track mouse movements. Some OSes do not provide an easy way
  // of tracking mouse movements across applications.  So we use a timer to
  // accomplish the same.  This could also be more efficient as you end up
  // getting a lot of notifications when tracking mouse movements.
  base::RepeatingTimer<PanelMouseWatcherTimer> timer_;

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
  // TODO(scottmg): NativeScreen is wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
}

void PanelMouseWatcherTimer::DoWork() {
  NotifyMouseMovement(GetMousePosition());
}
