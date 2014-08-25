// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_ATHENA_TEST_HELPER_H_
#define ATHENA_TEST_ATHENA_TEST_HELPER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tree_host.h"

namespace base {
class MessageLoopForUI;
class Thread;
}

namespace ui {
class ContextFactory;
class ScopedAnimationDurationScaleMode;
}

namespace aura {
class Window;
class TestScreen;
class WindowTreeHost;
namespace client {
class FocusClient;
}
}

namespace wm {
class InputMethodEventFilter;
}

namespace athena {
namespace test {

// A helper class owned by tests that does common initialization required for
// Athena use. This class creates a root window with clients and other objects
// that are necessary to run test on Athena.
class AthenaTestHelper {
 public:
  explicit AthenaTestHelper(base::MessageLoopForUI* message_loop);
  ~AthenaTestHelper();

  // Creates and initializes (shows and sizes) the RootWindow for use in tests.
  void SetUp(ui::ContextFactory* context_factory);

  // Clean up objects that are created for tests. This also deletes the Env
  // object.
  void TearDown();

  // Flushes message loop.
  void RunAllPendingInMessageLoop();

  aura::Window* GetRootWindow();
  aura::WindowTreeHost* GetHost();

 private:
  bool setup_called_;
  bool teardown_called_;

  base::MessageLoopForUI* message_loop_;

  scoped_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  scoped_ptr<base::Thread> file_thread_;

  DISALLOW_COPY_AND_ASSIGN(AthenaTestHelper);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_ATHENA_TEST_HELPER_H_
