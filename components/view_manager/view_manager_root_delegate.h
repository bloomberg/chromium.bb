// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_DELEGATE_H_

namespace view_manager {

class ViewManagerServiceImpl;

// A ViewManagerRootDelegate interface is implemented by an object that
// has the ViewManagerServiceImpl that is associated with the
// ViewManagerRootImpl that holds a pointer to this object. Typically, a
// ViewManagerRootDelegate will also manage the lifetime of the
// ViewManagerRootImpl and will delete the object when it gets informed of
// when the Display of the root is closed.
class ViewManagerRootDelegate {
 public:
  // Called when the window associated with the root is closed.
  virtual void OnDisplayClosed() = 0;

  // Returns the ViewManagerServiceImpl associated with the delegate.
  virtual ViewManagerServiceImpl* GetViewManagerService() = 0;

 protected:
  virtual ~ViewManagerRootDelegate() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_DELEGATE_H_
