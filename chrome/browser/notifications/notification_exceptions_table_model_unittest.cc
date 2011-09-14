// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_exceptions_table_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class NotificationExceptionsTableModelTest
    : public ChromeRenderViewHostTestHarness {
 public:
  NotificationExceptionsTableModelTest()
     : ui_thread_(BrowserThread::UI, MessageLoop::current()) {
  }

  virtual ~NotificationExceptionsTableModelTest() {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
    service_ = DesktopNotificationServiceFactory::GetForProfile(profile());
    ResetModel();
  }

  virtual void TearDown() {
    model_.reset(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
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
  HostContentSettingsMap::SettingsForOneType settings;
  service_->GetNotificationsSettings(&settings);
  EXPECT_EQ(5u, settings.size());
  EXPECT_EQ(5, model_->RowCount());

  model_->RemoveAll();
  EXPECT_EQ(0, model_->RowCount());

  service_->GetNotificationsSettings(&settings);
  EXPECT_EQ(0u, settings.size());
}

TEST_F(NotificationExceptionsTableModelTest, AlphabeticalOrder) {
  FillData();
  EXPECT_EQ(5, model_->RowCount());

  EXPECT_EQ(ASCIIToUTF16("http://allowed.com:80"),
            model_->GetText(0, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON),
            model_->GetText(0, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("http://denied.com:80"),
            model_->GetText(1, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON),
            model_->GetText(1, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("http://denied2.com:80"),
            model_->GetText(2, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON),
            model_->GetText(2, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("http://e-allowed2.com:80"),
            model_->GetText(3, IDS_EXCEPTIONS_HOSTNAME_HEADER));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON),
            model_->GetText(3, IDS_EXCEPTIONS_ACTION_HEADER));

  EXPECT_EQ(ASCIIToUTF16("http://f-denied3.com:80"),
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

  HostContentSettingsMap::SettingsForOneType settings;
  service_->GetNotificationsSettings(&settings);
  EXPECT_EQ(3u, settings.size());

  {
    RemoveRowsTableModel::Rows rows;
    rows.insert(0);
    rows.insert(1);
    rows.insert(2);
    model_->RemoveRows(rows);
  }
  EXPECT_EQ(0, model_->RowCount());
  service_->GetNotificationsSettings(&settings);
  EXPECT_EQ(0u, settings.size());
}
