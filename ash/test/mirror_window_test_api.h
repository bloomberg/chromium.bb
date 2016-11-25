// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_MIRROR_WINDOW_TEST_API_H_
#define ASH_TEST_MIRROR_WINDOW_TEST_API_H_

#include "base/macros.h"

namespace aura {
class Window;
class WindowTreeHost;
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

  const aura::WindowTreeHost* GetHost() const;

  int GetCurrentCursorType() const;

  // Returns the position of the hot point within the cursor. This is
  // unaffected by the cursor location.
  const gfx::Point& GetCursorHotPoint() const;

  // Returns the position of the cursor hot point in root window coordinates.
  // This should be the same as the native cursor location.
  gfx::Point GetCursorHotPointLocationInRootWindow() const;

  const aura::Window* GetCursorWindow() const;
  gfx::Point GetCursorLocation() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MirrorWindowTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_MIRROR_WINDOW_TEST_API_H_
