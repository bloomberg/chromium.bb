// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(OS_IOS)
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
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
#if defined(USE_AURA)
  base::FilePath pak_file;
  PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.AppendASCII("ui_test.pak");
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
#endif
}

ContentTestSuite::~ContentTestSuite() {
#if defined(USE_AURA)
  ui::ResourceBundle::CleanupSharedInstance();
#endif
}

void ContentTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  ContentTestSuiteBase::Initialize();

#if !defined(OS_IOS)
  // When running in a child process for Mac sandbox tests, the sandbox exists
  // to initialize GL, so don't do it here.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestChildProcess))
    gfx::GLSurface::InitializeOneOffForTests();
#endif

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestInitializationListener);
}

ContentClient* ContentTestSuite::CreateClientForInitialization() {
  return new TestContentClient();
}

}  // namespace content
