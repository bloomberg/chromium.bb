// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SEAT_H_
#define COMPONENTS_EXO_SEAT_H_

#include "base/observer_list.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"

namespace exo {

class SeatObserver;
class Surface;

// Seat object represent a group of input devices such as keyboard, pointer and
// touch devices and keeps track of input focus.
class Seat : public aura::client::FocusChangeObserver {
 public:
  Seat();
  ~Seat() override;

  void AddObserver(SeatObserver* observer);
  void RemoveObserver(SeatObserver* observer);

  // Returns currently focused surface. This is vertual so that we can override
  // the behavior for testing.
  virtual Surface* GetFocusedSurface();

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

 private:
  base::ObserverList<SeatObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(Seat);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SEAT_H_
