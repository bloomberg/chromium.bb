// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_test_base.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  // An AtExitManager instance is needed to construct message loops.
  base::AtExitManager at_exit;

  return mojo::test::RunAllTests(shell_handle);
}
