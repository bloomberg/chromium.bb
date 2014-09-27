// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/cc_test_suite.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread_id_name_manager.h"
#include "cc/test/paths.h"
#include "ui/gl/gl_surface.h"

namespace cc {

CCTestSuite::CCTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

CCTestSuite::~CCTestSuite() {}

void CCTestSuite::Initialize() {
  base::TestSuite::Initialize();
  gfx::GLSurface::InitializeOneOffForTests();
  CCPaths::RegisterPathProvider();

  message_loop_.reset(new base::MessageLoop);

  base::ThreadIdNameManager::GetInstance()->SetName(
      base::PlatformThread::CurrentId(),
      "Main");
}

void CCTestSuite::Shutdown() {
  message_loop_.reset();

  base::TestSuite::Shutdown();
}

}  // namespace cc
