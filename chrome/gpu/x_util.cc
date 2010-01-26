// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/x_util.h"

#include <X11/Xutil.h>

void ScopedPtrXFree::operator()(void* x) const {
  ::XFree(x);
}
