// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#if defined(OS_CHROMEOS)
#include <stdio.h>
#include <unistd.h>
#endif

#include "base/logging.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/test_content_client.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_aura_initializer.h"
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
  aura_initializer_.reset(new aura::test::TestAuraInitializer);
#endif
}

ContentTestSuite::~ContentTestSuite() {
}

static bool IsCrosPythonProcess() {
#if defined(OS_CHROMEOS)
  char buf[80];
  int num_read = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (num_read == -1)
    return false;
  buf[num_read] = 0;
  const char kPythonPrefix[] = "/python";
  return !strncmp(strrchr(buf, '/'), kPythonPrefix, sizeof(kPythonPrefix) - 1);
#endif  // defined(OS_CHROMEOS)
  return false;
}

void ContentTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  ContentTestSuiteBase::Initialize();

  // Initialize media library for unit tests. If we are auto test
  // (python process under chrome os), media library will be loaded to
  // chrome directly so don't load it here.
  if (!IsCrosPythonProcess())
    media::InitializeMediaLibraryForTesting();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestInitializationListener);
}

ContentClient* ContentTestSuite::CreateClientForInitialization() {
  return new TestContentClient();
}

}  // namespace content
