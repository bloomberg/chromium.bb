// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SEAT_H_
#define COMPONENTS_EXO_SEAT_H_

#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/events/event_handler.h"

namespace ui {
enum class DomCode;
class KeyEvent;
}  // namespace ui

namespace exo {
class SeatObserver;
class Surface;

// Seat object represent a group of input devices such as keyboard, pointer and
// touch devices and keeps track of input focus.
class Seat : public aura::client::FocusChangeObserver, public ui::EventHandler {
 public:
  Seat();
  ~Seat() override;

  void AddObserver(SeatObserver* observer);
  void RemoveObserver(SeatObserver* observer);

  // Returns currently focused surface. This is vertual so that we can override
  // the behavior for testing.
  virtual Surface* GetFocusedSurface();

  // Returns currently pressed keys.
  const base::flat_set<ui::DomCode>& pressed_keys() const {
    return pressed_keys_;
  }

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  base::ObserverList<SeatObserver> observers_;
  base::flat_set<ui::DomCode> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(Seat);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SEAT_H_
