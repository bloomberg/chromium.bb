// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"
#endif

namespace mojo {

class WindowManagerTestSuite : public base::TestSuite {
 public:
  WindowManagerTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}
  virtual ~WindowManagerTestSuite() {}

 protected:
  virtual void Initialize() OVERRIDE {
#if defined(USE_X11)
    // Each test ends up creating a new thread for the native viewport service.
    // In other words we'll use X on different threads, so tell it that.
    gfx::InitializeThreadedX11();
#endif
    base::TestSuite::Initialize();
    gfx::GLSurface::InitializeOneOffForTests();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerTestSuite);
};

}  // namespace mojo

int main(int argc, char** argv) {
  mojo::WindowManagerTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&TestSuite::Run, base::Unretained(&test_suite)));
}
