// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_
#define MOJO_SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_

#include "mojo/services/view_manager/view_manager_export.h"

namespace gfx {
class Rect;
}

namespace mojo {
namespace service {

class ServerView;

class MOJO_VIEW_MANAGER_EXPORT ServerViewDelegate {
 public:
  // Invoked at the end of the View's destructor (after it has been removed from
  // the hierarchy).
  virtual void OnViewDestroyed(const ServerView* view) = 0;

  virtual void OnWillChangeViewHierarchy(const ServerView* view,
                                         const ServerView* new_parent,
                                         const ServerView* old_parent) = 0;

  virtual void OnViewHierarchyChanged(const ServerView* view,
                                      const ServerView* new_parent,
                                      const ServerView* old_parent) = 0;

  virtual void OnViewBoundsChanged(const ServerView* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) = 0;

  virtual void OnViewSurfaceIdChanged(const ServerView* view) = 0;

  virtual void OnWillChangeViewVisibility(const ServerView* view) = 0;

 protected:
  virtual ~ServerViewDelegate() {}
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_SERVER_VIEW_DELEGATE_H_
