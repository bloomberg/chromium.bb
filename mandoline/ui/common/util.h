// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_COMMON_UTIL_H_
#define MANDOLINE_UI_COMMON_UTIL_H_

#include <vector>
#include "ui/gfx/display.h"

namespace mus {
class Window;
}

namespace mandoline {

std::vector<gfx::Display> GetDisplaysFromWindow(mus::Window* window);

}  // namespace mandoline

#endif  // MANDOLINE_UI_COMMON_UTIL_H_