// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/logging.h"
#include "content/browser/mock_content_browser_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/test/test_content_client.h"
#include "content/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif
#include "ui/compositor/compositor_setup.h"


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

ContentTestSuite::ContentTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

ContentTestSuite::~ContentTestSuite() {
}

void ContentTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  base::TestSuite::Initialize();

  TestContentClient client_for_init;
  content::SetContentClient(&client_for_init);
  content::RegisterContentSchemes(false);
  content::SetContentClient(NULL);

  content::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Mock out the compositor on platforms that use it.
  ui::SetupTestCompositor();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestInitializationListener);
}

