// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_MIRROR_WINDOW_TEST_API_H_
#define ASH_TEST_MIRROR_WINDOW_TEST_API_H_

#include "base/basictypes.h"

namespace aura {
class RootWindow;
class Window;
}

namespace gfx {
class Point;
}

namespace ash {
namespace test {

class MirrorWindowTestApi {
 public:
  MirrorWindowTestApi() {}
  ~MirrorWindowTestApi() {}

  const aura::RootWindow* GetRootWindow() const;

  int GetCurrentCursorType() const;
  const gfx::Point& GetCursorHotPoint() const;
  const aura::Window* GetCursorWindow() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MirrorWindowTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_MIRROR_WINDOW_TEST_API_H_
