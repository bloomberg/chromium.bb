// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_OBSERVER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_OBSERVER_H_

namespace mus {

class Window;

class WindowTreeConnectionObserver {
 public:
  virtual void OnWindowTreeFocusChanged(Window* gained_focus,
                                        Window* lost_focus) {}

 protected:
  virtual ~WindowTreeConnectionObserver() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TREE_CONNECTION_OBSERVER_H_
