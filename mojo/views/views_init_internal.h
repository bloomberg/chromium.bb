// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_VIEWS_VIEWS_INIT_INTERNAL_H_
#define MOJO_VIEWS_VIEWS_INIT_INTERNAL_H_

#include "mojo/views/mojo_views_export.h"

namespace mojo {

// This is in a separate target so that we only initialize some common state
// once.
// Do not use this, it is called internally by ViewsInit.
MOJO_VIEWS_EXPORT void InitViews();

}  // namespace mojo

#endif  // MOJO_VIEWS_VIEWS_INIT_INTERNAL_H_
