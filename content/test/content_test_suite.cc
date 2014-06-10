// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_content_client_initializer.h"
#include "gpu/config/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if !defined(OS_IOS)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "media/base/media.h"
#include "ui/gl/gl_surface.h"
#endif

namespace {

class TestInitializationListener : public testing::EmptyTestEventListener {
 public:
  TestInitializationListener() : test_content_client_initializer_(NULL) {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    test_content_client_initializer_ =
        new content::TestContentClientInitializer();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    delete test_content_client_initializer_;
  }

 private:
  content::TestContentClientInitializer* test_content_client_initializer_;

  DISALLOW_COPY_AND_ASSIGN(TestInitializationListener);
};

}  // namespace

namespace content {

ContentTestSuite::ContentTestSuite(int argc, char** argv)
    : ContentTestSuiteBase(argc, argv) {
}

ContentTestSuite::~ContentTestSuite() {
}

void ContentTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

#if defined(OS_WIN)
  gfx::InitDeviceScaleFactor(1.0f);
#endif

  ContentTestSuiteBase::Initialize();
  {
    ContentClient client;
    ContentTestSuiteBase::RegisterContentSchemes(&client);
  }
  RegisterPathProvider();
#if !defined(OS_IOS)
  media::InitializeMediaLibraryForTesting();
  // When running in a child process for Mac sandbox tests, the sandbox exists
  // to initialize GL, so don't do it here.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTestChildProcess)) {
    gfx::GLSurface::InitializeOneOffForTests();
    gpu::ApplyGpuDriverBugWorkarounds(CommandLine::ForCurrentProcess());
  }
#endif
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestInitializationListener);
}

}  // namespace content
