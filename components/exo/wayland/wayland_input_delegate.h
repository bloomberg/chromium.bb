// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_INPUT_DELEGATE_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_INPUT_DELEGATE_H_

#include "base/observer_list.h"
#include "base/time/time.h"

namespace exo {
namespace wayland {

class WaylandInputDelegate {
 public:
  class Observer {
   public:
    virtual void OnDelegateDestroying(WaylandInputDelegate* delegate) = 0;
    virtual void OnSendTimestamp(base::TimeTicks time_stamp) = 0;

   protected:
    virtual ~Observer() = default;
  };

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  void SendTimestamp(base::TimeTicks time_stamp);

 protected:
  WaylandInputDelegate();
  virtual ~WaylandInputDelegate();

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputDelegate);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_INPUT_DELEGATE_H_
