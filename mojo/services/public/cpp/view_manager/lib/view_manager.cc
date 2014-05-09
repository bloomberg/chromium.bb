// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"

namespace mojo {
namespace services {
namespace view_manager {

ViewManager::ViewManager(Shell* shell)
    : shell_(shell) {}

ViewManager::~ViewManager() {}

void ViewManager::Init() {
  synchronizer_.reset(new ViewManagerSynchronizer(this));
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
