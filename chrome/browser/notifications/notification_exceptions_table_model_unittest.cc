// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_exceptions_table_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class NotificationExceptionsTableModelTest : public RenderViewHostTestHarness {
 public:
  NotificationExceptionsTableModelTest()
     : ui_thread_(BrowserThread::UI, MessageLoop::current()) {
  }

  virtual ~NotificationExceptionsTableModelTest() {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    service_ = profile()->GetDesktopNotificationService();
    ResetModel();
  }

  virtual void TearDown() {
    model_.reset(NULL);
    RenderViewHostTestHarness::TearDown();
  }

  virtual void ResetModel() {
    model_.reset(new NotificationExceptionsTableModel(service_));
  }

  virtual void FillData() {
    service_->GrantPermission(GURL("http://e-allowed2.com"));
    service_->GrantPermission(GURL("http://allowed.com"));

    service_->DenyPermission(GURL("http://denied2.com"));
    service_->DenyPermission(GURL("http://denied.com"));
    service_->DenyPermission(GURL("http://f-denied3.com"));

    ResetModel();
  }

 protected:
  BrowserThread ui_thread_;
  scoped_ptr<NotificationExceptionsTableModel> model_;
  DesktopNotificationService* service_;
};

TEST_F(NotificationExceptionsTableModelTest, CanCreate) {
  EXPECT_EQ(0, model_->RowCount());
}

TEST_F(NotificationExceptionsTableModelTest, RemoveAll) {
  FillData();
  EXPECT_EQ(2u, service_->GetAllowedOrigins().size());
  EXPECT_EQ(3u, service_->GetBlockedOrigins().size());
  EXPECT_EQ(5, model_->RowCount());

  model_->RemoveAll();
  EXPECT_EQ(0, model_->RowCount());

  EXPECT_EQ(0u, service_->GetAllowedOrigins().size());
  EXPECT_EQ(0u, service_->GetBlockedOrigins().size());
}

TEST_F(NotificationExceptionsTableModelTest, AlphabeticalOrder) {
  FillData();
  EXPECT_EQ(5, model_->RowCount());

  EXPECT_EQ(ASCIIToUTF16("allowed.com"),
            model_->GetText(0, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON),
            model_->GetText(0, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("denied.com"),
            model_->GetText(1, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON),
            model_->GetText(1, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("denied2.com"),
            model_->GetText(2, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON),
            model_->GetText(2, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("e-allowed2.com"),
            model_->GetText(3, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON),
            model_->GetText(3, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("f-denied3.com"),
            model_->GetText(4, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON),
            model_->GetText(4, IDS_EXCEPTIONS_ACTION_HEADER));
}

TEST_F(NotificationExceptionsTableModelTest, RemoveRows) {
  FillData();
  EXPECT_EQ(5, model_->RowCount());

  {
    RemoveRowsTableModel::Rows rows;
    rows.insert(0);  // allowed.com
    rows.insert(3);  // e-allowed2.com
    model_->RemoveRows(rows);
  }
  EXPECT_EQ(3, model_->RowCount());
  EXPECT_EQ(0u, service_->GetAllowedOrigins().size());
  EXPECT_EQ(3u, service_->GetBlockedOrigins().size());

  {
    RemoveRowsTableModel::Rows rows;
    rows.insert(0);
    rows.insert(1);
    rows.insert(2);
    model_->RemoveRows(rows);
  }
  EXPECT_EQ(0, model_->RowCount());
  EXPECT_EQ(0u, service_->GetAllowedOrigins().size());
  EXPECT_EQ(0u, service_->GetBlockedOrigins().size());
}
