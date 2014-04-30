// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"

namespace mojo {
namespace services {
namespace view_manager {

ViewManagerPrivate::ViewManagerPrivate(ViewManager* manager)
    : manager_(manager) {}
ViewManagerPrivate::~ViewManagerPrivate() {}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
