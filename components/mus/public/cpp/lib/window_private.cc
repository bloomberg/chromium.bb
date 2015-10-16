// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/window_private.h"

namespace mus {

WindowPrivate::WindowPrivate(Window* window) : window_(window) {
  CHECK(window);
}

WindowPrivate::~WindowPrivate() {}

// static
Window* WindowPrivate::LocalCreate() {
  return new Window;
}

}  // namespace mus
