// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_DELEGATE_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_DELEGATE_H_

#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace services {
namespace view_manager {

class View;
class ViewId;

class MOJO_VIEW_MANAGER_EXPORT ViewDelegate {
 public:
  // Returns the ViewId for the view.
  virtual ViewId GetViewId(const View* view) const = 0;

  // Invoked when the hierarchy has changed.
  virtual void OnViewHierarchyChanged(const ViewId& view,
                                      const ViewId& new_parent,
                                      const ViewId& old_parent) = 0;

 protected:
  virtual ~ViewDelegate() {}
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_DELEGATE_H_
