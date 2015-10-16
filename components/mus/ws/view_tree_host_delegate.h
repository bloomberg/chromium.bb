// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_VIEW_TREE_HOST_DELEGATE_H_
#define COMPONENTS_MUS_WS_VIEW_TREE_HOST_DELEGATE_H_

namespace mus {

class ViewTreeImpl;

// A ViewTreeHostDelegate interface is implemented by an object that
// has the WindowTreeImpl that is associated with the ViewTreeHostImpl that
// holds
// a pointer to this object. Typically, a ViewTreeHostDelegate will also manage
// the lifetime of the ViewTreeHostImpl and will delete the object when it gets
// informed of when the Display of the root is closed.
class ViewTreeHostDelegate {
 public:
  // Called when the window associated with the root is completely initialized
  // (i.e. the ViewportMetrics for the display is known).
  virtual void OnDisplayInitialized() = 0;

  // Called when the window associated with the root is closed.
  virtual void OnDisplayClosed() = 0;

  // Returns the WindowTreeImpl associated with the delegate.
  virtual ViewTreeImpl* GetWindowTree() = 0;

 protected:
  virtual ~ViewTreeHostDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_VIEW_TREE_HOST_DELEGATE_H_
