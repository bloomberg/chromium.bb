// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace mojo {
class Shell;
namespace services {
namespace view_manager {

class ViewManagerSynchronizer;

// Approximately encapsulates the View Manager service.
// Owns a synchronizer that keeps a client model in sync with the service.
// Owned by the creator.
//
// TODO: displays
class ViewManager {
 public:
  explicit ViewManager(Shell* shell);
  ~ViewManager();

 private:
  friend class ViewManagerPrivate;

  Shell* shell_;
  scoped_ptr<ViewManagerSynchronizer> synchronizer_;

  DISALLOW_COPY_AND_ASSIGN(ViewManager);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
