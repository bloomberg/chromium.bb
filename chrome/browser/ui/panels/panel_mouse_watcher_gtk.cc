// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "chrome/browser/ui/panels/panel_mouse_watcher_gtk.h"

#include "base/memory/singleton.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "ui/gfx/screen.h"

// A timer based implementation of PanelMouseWatcher.  Currently used on Gtk
// but could be used on any platform.
class PanelMouseWatcherGtk : public PanelMouseWatcher {
 public:
  // Returns the singleton instance.
  static PanelMouseWatcherGtk* GetInstance();

  virtual ~PanelMouseWatcherGtk();

 protected:
  virtual void Start();
  virtual void Stop();

 private:
  // Specifies the rate at which we want to sample the mouse position.
  static const int kMousePollingIntervalMs = 250;

  PanelMouseWatcherGtk();
  friend struct DefaultSingletonTraits<PanelMouseWatcherGtk>;

  // Timer callback function.
  void DoWork();
  friend class base::RepeatingTimer<PanelMouseWatcherGtk>;

  // Timer used to track mouse movements.  Gtk does not provide an easy way of
  // tracking mouse movements across applications.  So we use a timer to
  // accomplish the same.  This could also be more efficient as you end up
  // getting a lot of notifications when tracking mouse movements.
  base::RepeatingTimer<PanelMouseWatcherGtk> timer_;

  DISALLOW_COPY_AND_ASSIGN(PanelMouseWatcherGtk);
};

// static
PanelMouseWatcher* PanelMouseWatcher::GetInstance() {
  return PanelMouseWatcherGtk::GetInstance();
}

// static
PanelMouseWatcherGtk* PanelMouseWatcherGtk::GetInstance() {
  return Singleton<PanelMouseWatcherGtk>::get();
}

PanelMouseWatcherGtk::PanelMouseWatcherGtk() : PanelMouseWatcher() {
}

PanelMouseWatcherGtk::~PanelMouseWatcherGtk() {
}

void PanelMouseWatcherGtk::Start() {
  DCHECK(!timer_.IsRunning());
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kMousePollingIntervalMs),
               this, &PanelMouseWatcherGtk::DoWork);
}

void PanelMouseWatcherGtk::Stop() {
  DCHECK(timer_.IsRunning());
  timer_.Stop();
}

void PanelMouseWatcherGtk::DoWork() {
  HandleMouseMovement(gfx::Screen::GetCursorScreenPoint());
}
