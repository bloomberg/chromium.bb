// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/mock_content_browser_client.h"
#include "content/common/content_client.h"
#include "content/common/notification_service.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestContentClientInitializer : public testing::EmptyTestEventListener {
 public:
  TestContentClientInitializer() {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    notification_service_.reset(new NotificationService());

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
  scoped_ptr<NotificationService> notification_service_;
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
  base::TestSuite::Initialize();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestContentClientInitializer);
}
