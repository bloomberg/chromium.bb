// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_HOST_DELEGATE_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_HOST_DELEGATE_H_

namespace mus {

class WindowTreeImpl;

// A WindowTreeHostDelegate interface is implemented by an object that
// has the WindowTreeImpl that is associated with the WindowTreeHostImpl that
// holds
// a pointer to this object. Typically, a WindowTreeHostDelegate will also
// manage the lifetime of the WindowTreeHostImpl and will delete the object when
// it get informed of when the Display of the root is closed.
class WindowTreeHostDelegate {
 public:
  // Called when the window associated with the root is completely initialized
  // (i.e. the ViewportMetrics for the display is known).
  virtual void OnDisplayInitialized() = 0;

  // Called when the window associated with the root is closed.
  virtual void OnDisplayClosed() = 0;

  // Returns the WindowTreeImpl associated with the delegate.
  virtual WindowTreeImpl* GetWindowTree() = 0;

 protected:
  virtual ~WindowTreeHostDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_HOST_DELEGATE_H_
