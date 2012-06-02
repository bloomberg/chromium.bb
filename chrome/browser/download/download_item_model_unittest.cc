// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include <vector>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/test/mock_download_item.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/font.h"

using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace {

// All the interrupt reasons we know of. The reason codes are unfortunately
// sparse, making this array necessary.
content::DownloadInterruptReason kInterruptReasons[] = {
  content::DOWNLOAD_INTERRUPT_REASON_NONE,
#define INTERRUPT_REASON(name,value)            \
  content::DOWNLOAD_INTERRUPT_REASON_##name,
#include "content/public/browser/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};

class DownloadItemModelTest : public testing::Test {
 public:
  DownloadItemModelTest() {}

  virtual ~DownloadItemModelTest() {
  }

 protected:
  void SetupDownloadItemDefaults(const GURL& url) {
    ON_CALL(item_, GetReceivedBytes()).WillByDefault(Return(1024));
    ON_CALL(item_, GetTotalBytes()).WillByDefault(Return(2048));
    ON_CALL(item_, IsInProgress()).WillByDefault(Return(false));
    ON_CALL(item_, TimeRemaining(_)).WillByDefault(Return(false));
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, AllDataSaved()).WillByDefault(Return(false));
    ON_CALL(item_, GetOpenWhenComplete()).WillByDefault(Return(false));
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
    ON_CALL(item_, GetState())
        .WillByDefault(Return(content::DownloadItem::IN_PROGRESS));
    ON_CALL(item_, GetURL()).WillByDefault(ReturnRefOfCopy(url));
    ON_CALL(item_, GetFileNameToReportUser())
        .WillByDefault(Return(FilePath(FILE_PATH_LITERAL("foo.bar"))));
    ON_CALL(item_, GetTargetDisposition())
        .WillByDefault(
            Return(content::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  }

  void SetupInterruptedDownloadItem(content::DownloadInterruptReason reason) {
    EXPECT_CALL(item_, GetLastReason()).WillRepeatedly(Return(reason));
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(
            (reason == content::DOWNLOAD_INTERRUPT_REASON_NONE) ?
                content::DownloadItem::IN_PROGRESS :
                content::DownloadItem::INTERRUPTED));
    model_.reset(new DownloadItemModel(&item_));
  }

  void TestStatusText(
      content::DownloadInterruptReason reason) {
    SetupInterruptedDownloadItem(reason);

    int64 size = item_.GetReceivedBytes();
    int64 total = item_.GetTotalBytes();

    ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
    string16 simple_size = ui::FormatBytesWithUnits(size, amount_units, false);
    string16 simple_total = base::i18n::GetDisplayStringInLTRDirectionality(
        ui::FormatBytesWithUnits(total, amount_units, true));

    string16 status_text;
    if (reason != content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED) {
      string16 size_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_RECEIVED_SIZE, simple_size, simple_total);
      status_text = size_text;
      if (reason != content::DOWNLOAD_INTERRUPT_REASON_NONE) {
        status_text = status_text + ASCIIToUTF16(" ") +
            DownloadItemModel::InterruptReasonStatusMessage(reason);
      }
    } else {
      status_text = DownloadItemModel::InterruptReasonStatusMessage(reason);
    }

    DVLOG(1) << " " << __FUNCTION__ << "()" << " '" << status_text << "'";

    string16 text = model_->GetStatusText();
    EXPECT_EQ(status_text, text);
    ::testing::Mock::VerifyAndClearExpectations(&item_);
  }

  void TestTooltipWithFixedWidth(const gfx::Font& font, int width) {
    content::DownloadInterruptReason reason = item_.GetLastReason();
    string16 tooltip = model_->GetTooltipText(font, width);
    std::vector<string16> lines;

    DVLOG(1) << "Tooltip: '" << tooltip << "'";
    Tokenize(tooltip, ASCIIToUTF16("\n"), &lines);
    for (unsigned i = 0; i < lines.size(); ++i)
      EXPECT_GE(width, font.GetStringWidth(lines[i]));
    if (reason == content::DOWNLOAD_INTERRUPT_REASON_NONE ||
        reason == content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED) {
      // Only expect the filename for non-interrupted and canceled downloads.
      EXPECT_EQ(1u, lines.size());
    } else {
      EXPECT_EQ(2u, lines.size());
    }
  }

  void TestTooltipText(content::DownloadInterruptReason reason) {
    const int kSmallTooltipWidth = 40;
    const int kLargeTooltipWidth = 1000;
    gfx::Font font;

    SetupInterruptedDownloadItem(reason);
    TestTooltipWithFixedWidth(font, kSmallTooltipWidth);
    TestTooltipWithFixedWidth(font, kLargeTooltipWidth);
    ::testing::Mock::VerifyAndClearExpectations(&item_);
  }

 private:
  scoped_ptr<DownloadItemModel> model_;

  NiceMock<content::MockDownloadItem> item_;
};

}  // namespace

// Test download error messages.
TEST_F(DownloadItemModelTest, InterruptStatus) {
  SetupDownloadItemDefaults(GURL("http://foo.bar/"));
  // We are iterating through all the reason codes to make sure all message
  // strings have the correct format placeholders. If not, GetStringFUTF16()
  // will throw up.
  for (unsigned i = 0; i < arraysize(kInterruptReasons); ++i)
    TestStatusText(kInterruptReasons[i]);
}

TEST_F(DownloadItemModelTest, InterruptTooltip) {
  SetupDownloadItemDefaults(GURL("http://foo.bar/"));
  // Iterating through all the reason codes to exercise all the status strings.
  // We also check whether the strings are properly elided.
  for (unsigned i = 0; i < arraysize(kInterruptReasons); ++i)
    TestTooltipText(kInterruptReasons[i]);
}
