// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_mojo_shell_context.h"

#include "content/browser/mojo/mojo_shell_context.h"

namespace content {

TestMojoShellContext::TestMojoShellContext()
    : context_(new MojoShellContext) {}

TestMojoShellContext::~TestMojoShellContext() {}

}  // namespace content
