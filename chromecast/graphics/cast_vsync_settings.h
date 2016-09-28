// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_VSYNC_SETTINGS_H_
#define CHROMECAST_GRAPHICS_CAST_VSYNC_SETTINGS_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace chromecast {

// Provides a central place to manage VSync interval, supports observers
// to be notified of changes.
// TODO(halliwell): plumb this via Ozone or ui::Display instead.
class CastVSyncSettings {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnVSyncIntervalChanged(base::TimeDelta interval) = 0;
  };

  static CastVSyncSettings* GetInstance();

  base::TimeDelta GetVSyncInterval() const;
  void SetVSyncInterval(base::TimeDelta interval);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend struct base::DefaultLazyInstanceTraits<CastVSyncSettings>;

  CastVSyncSettings();
  ~CastVSyncSettings();

  base::TimeDelta interval_;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CastVSyncSettings);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_VSYNC_SETTINGS_H_
