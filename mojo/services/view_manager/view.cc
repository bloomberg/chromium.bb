// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view.h"

#include "mojo/services/view_manager/node.h"

namespace mojo {
namespace services {
namespace view_manager {

View::View(const ViewId& id) : id_(id), node_(NULL) {}

View::~View() {
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
