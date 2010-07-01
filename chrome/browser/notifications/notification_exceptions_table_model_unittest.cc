// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_exceptions_table_model.h"

#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class NotificationExceptionsTableModelTest
  : public RenderViewHostTestHarness {
 public:
  NotificationExceptionsTableModelTest()
     : ui_thread_(ChromeThread::UI, MessageLoop::current()) {
  }

  virtual ~NotificationExceptionsTableModelTest() {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    ResetModel();
  }

  virtual void TearDown() {
    model_.reset(NULL);
    RenderViewHostTestHarness::TearDown();
  }

  virtual void ResetModel() {
    model_.reset(new NotificationExceptionsTableModel(
        profile()->GetDesktopNotificationService()));
  }

 protected:
  ChromeThread ui_thread_;
  scoped_ptr<NotificationExceptionsTableModel> model_;
};

TEST_F(NotificationExceptionsTableModelTest, CanCreate) {
  EXPECT_EQ(0, model_->RowCount());
}
