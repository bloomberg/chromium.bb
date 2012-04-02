// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "content/test/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;

content::DownloadId::Domain kValidIdDomain = "valid DownloadId::Domain";

class DownloadItemModelTest : public testing::Test {
 public:
  DownloadItemModelTest() {}

  virtual ~DownloadItemModelTest() {
  }

 protected:
  void SetupDownloadItemDefaults() {
    ON_CALL(item_, GetReceivedBytes()).WillByDefault(Return(1024));
    ON_CALL(item_, GetTotalBytes()).WillByDefault(Return(2048));
    ON_CALL(item_, IsInProgress()).WillByDefault(Return(false));
    ON_CALL(item_, TimeRemaining(_)).WillByDefault(Return(false));
    ON_CALL(item_, GetState()).
        WillByDefault(Return(content::DownloadItem::INTERRUPTED));
    ON_CALL(item_, PromptUserForSaveLocation()).
        WillByDefault(Return(false));
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, AllDataSaved()).WillByDefault(Return(false));
    ON_CALL(item_, GetOpenWhenComplete()).WillByDefault(Return(false));
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
  }

  void SetupDownloadItem(const GURL& url,
                         content::DownloadInterruptReason reason) {
    model_.reset(new DownloadItemModel(&item_));
    EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRefOfCopy(url));
    EXPECT_CALL(item_, GetLastReason()).WillOnce(Return(reason));
  }

  void TestDownloadItemModelStatusText(
      const GURL& url, content::DownloadInterruptReason reason) {
    SetupDownloadItem(url, reason);

    int64 size = item_.GetReceivedBytes();
    int64 total = item_.GetTotalBytes();
    bool know_size = (total > 0);

    ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
    string16 simple_size = ui::FormatBytesWithUnits(size, amount_units, false);
    string16 simple_total = base::i18n::GetDisplayStringInLTRDirectionality(
        ui::FormatBytesWithUnits(total, amount_units, true));

    string16 status_text;
    string16 size_text;

    status_text = DownloadItemModel::InterruptReasonStatusMessage(reason);

    if (know_size) {
      size_text = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_RECEIVED_SIZE,
                                             simple_size,
                                             simple_total);
    } else {
      size_text = ui::FormatBytes(size);
    }
    size_text = size_text + ASCIIToUTF16(" ");
    if (reason != content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED)
      status_text = size_text + status_text;

    DVLOG(2) << " " << __FUNCTION__ << "()" << " '" << status_text << "'";

    string16 text = model_->GetStatusText();

    EXPECT_EQ(status_text, text);
  }

  void TestDownloadItemModelStatusTextAllReasons(const GURL& url) {
    SetupDownloadItemDefaults();

#define INTERRUPT_REASON(name, value)  \
    TestDownloadItemModelStatusText(url, \
                                    content::DOWNLOAD_INTERRUPT_REASON_##name);

  #include "content/public/browser/download_interrupt_reason_values.h"

#undef INTERRUPT_REASON
  }

 private:
  scoped_ptr<DownloadItemModel> model_;

  NiceMock<content::MockDownloadItem> item_;
};

// Test download error messages.
TEST_F(DownloadItemModelTest, Interrupts) {
  TestDownloadItemModelStatusTextAllReasons(GURL("http://foo.bar/"));
}
