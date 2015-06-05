// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

namespace mojo {

extern int g_application_runner_argc;
extern const char* const* g_application_runner_argv;

}

#if !defined(OS_WIN)
extern "C" {
__attribute__((visibility("default"))) void InitCommandLineArgs(
    int argc, const char* const* argv) {
  mojo::g_application_runner_argc = argc;
  mojo::g_application_runner_argv = argv;
}
}
#endif
