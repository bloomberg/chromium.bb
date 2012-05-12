// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "ui/gl/gl_surface.h"

#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_util.h"
#endif

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
#if defined(TOOLKIT_GTK)
    gfx::GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());
#endif
  gfx::GLSurface::InitializeOneOff();
  ::gles2::Initialize();
  MessageLoop::Type message_loop_type = MessageLoop::TYPE_UI;
  MessageLoop main_message_loop(message_loop_type);
  return GLTestHelper::RunTests(argc, argv);
}


