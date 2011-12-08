// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/mock_content_browser_client.h"
#include "content/browser/notification_service_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/test/gfx_test_utils.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

class TestContentClientInitializer : public testing::EmptyTestEventListener {
 public:
  TestContentClientInitializer() {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    notification_service_.reset(new NotificationServiceImpl());

    DCHECK(!content::GetContentClient());
    content_client_.reset(new TestContentClient);
    content::SetContentClient(content_client_.get());

    content_browser_client_.reset(new content::MockContentBrowserClient());
    content_client_->set_browser(content_browser_client_.get());
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    notification_service_.reset();

    DCHECK_EQ(content_client_.get(), content::GetContentClient());
    content::SetContentClient(NULL);
    content_client_.reset();

    content_browser_client_.reset();
  }

 private:
  scoped_ptr<NotificationServiceImpl> notification_service_;
  scoped_ptr<content::ContentClient> content_client_;
  scoped_ptr<content::ContentBrowserClient> content_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(TestContentClientInitializer);
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

  content::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Mock out the compositor on platforms that use it.
  ui::gfx_test_utils::SetupTestCompositor();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestContentClientInitializer);
}
