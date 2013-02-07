// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

int main(int argc, char** argv) {
  // On Android, AtExitManager is created in
  // testing/android/native_test_wrapper.cc before main() is called.
  // The same thing is also done in base/test/test_suite.cc
#if !defined(OS_ANDROID)
  base::AtExitManager exit_manager;
#endif
  CommandLine::Init(argc, argv);
  gfx::InitializeGLBindings(gfx::kGLImplementationMockGL);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
