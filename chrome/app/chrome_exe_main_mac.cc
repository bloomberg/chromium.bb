// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all Mac Chromium processes, including the outer app
// bundle (browser) and helper app (renderer, plugin, and friends).

#include <stdlib.h>

extern "C" {
int ChromeMain(int argc, char** argv);
}  // extern "C"

__attribute__((visibility("default")))
int main(int argc, char* argv[]) {
  int rv = ChromeMain(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}
