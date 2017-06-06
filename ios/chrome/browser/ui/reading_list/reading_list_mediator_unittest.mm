// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ReadingListMediatorTest : public PlatformTest {
 public:
  ReadingListMediatorTest() {
    std::unique_ptr<base::SimpleTestClock> clock =
        base::MakeUnique<base::SimpleTestClock>();
    clock_ = clock.get();
    model_ = base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr,
                                                    std::move(clock));

    no_title_entry_url_ = GURL("http://chromium.org/unread3");
    // The first 3 have the same update time on purpose.
    model_->AddEntry(GURL("http://chromium.org/unread1"), "unread1",
                     reading_list::ADDED_VIA_CURRENT_APP);
    model_->AddEntry(GURL("http://chromium.org/read1"), "read1",
                     reading_list::ADDED_VIA_CURRENT_APP);
    model_->SetReadStatus(GURL("http://chromium.org/read1"), true);
    model_->AddEntry(GURL("http://chromium.org/unread2"), "unread2",
                     reading_list::ADDED_VIA_CURRENT_APP);
    clock_->Advance(base::TimeDelta::FromMilliseconds(10));
    model_->AddEntry(no_title_entry_url_, "",
                     reading_list::ADDED_VIA_CURRENT_APP);
    clock_->Advance(base::TimeDelta::FromMilliseconds(10));
    model_->AddEntry(GURL("http://chromium.org/read2"), "read2",
                     reading_list::ADDED_VIA_CURRENT_APP);
    model_->SetReadStatus(GURL("http://chromium.org/read2"), true);

    mediator_ = [[ReadingListMediator alloc] initWithModel:model_.get()];
  }

 protected:
  std::unique_ptr<ReadingListModelImpl> model_;
  ReadingListMediator* mediator_;
  base::SimpleTestClock* clock_;
  GURL no_title_entry_url_;
};

TEST_F(ReadingListMediatorTest, fillItems) {
  // Setup.
  NSMutableArray<ReadingListCollectionViewItem*>* readArray =
      [NSMutableArray array];
  NSMutableArray<ReadingListCollectionViewItem*>* unreadArray =
      [NSMutableArray array];

  // Action.
  [mediator_ fillReadItems:readArray unreadItems:unreadArray];

  // Tests.
  EXPECT_EQ(3U, [unreadArray count]);
  EXPECT_EQ(2U, [readArray count]);
  EXPECT_TRUE([unreadArray[0].title
      isEqualToString:base::SysUTF16ToNSString(url_formatter::FormatUrl(
                          no_title_entry_url_.GetOrigin()))]);
  EXPECT_TRUE([readArray[0].title isEqualToString:@"read2"]);
  EXPECT_TRUE([readArray[1].title isEqualToString:@"read1"]);
}
