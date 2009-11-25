// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#include <GLES2/gl2.h>
#include "gpu/command_buffer/client/gles2_demo_cc.h"

void GLFromCPPTestFunction() {
  static bool foo = true;
  foo = !foo;
  glClearColor(
      foo ? 1.0f : 0.0f,
      foo ? 0.0f : 1.0f,
      1.0f,
      1.0f);
}


