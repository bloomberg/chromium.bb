// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TRANSIENT_WINDOW_OBSERVER_H_
#define COMPONENTS_MUS_WS_TRANSIENT_WINDOW_OBSERVER_H_

namespace mus {
namespace ws {

class TransientWindowObserver {
 public:
  // Called when a transient child is added to |window|.
  virtual void OnTransientChildAdded(ServerWindow* window,
                                     ServerWindow* transient_child) = 0;

  // Called when a transient child is removed from |window|.
  virtual void OnTransientChildRemoved(ServerWindow* window,
                                       ServerWindow* transient_child) = 0;

 protected:
  virtual ~TransientWindowObserver() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TRANSIENT_WINDOW_OBSERVER_H_
