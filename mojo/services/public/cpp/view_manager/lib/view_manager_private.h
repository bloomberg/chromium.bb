// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_

#include "base/basictypes.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewManagerSynchronizer;

class ViewManagerPrivate {
 public:
  explicit ViewManagerPrivate(ViewManager* manager);
  ~ViewManagerPrivate();

  ViewManagerSynchronizer* synchronizer() {
    return manager_->synchronizer_.get();
  }
  Shell* shell() { return manager_->shell_; }

 private:
  ViewManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerPrivate);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_
