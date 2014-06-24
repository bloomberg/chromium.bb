// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/views/views_init.h"

#include "mojo/views/views_init_internal.h"

namespace mojo {

ViewsInit::ViewsInit() {
  InitViews();
}

ViewsInit::~ViewsInit() {
}

}  // namespace mojo
