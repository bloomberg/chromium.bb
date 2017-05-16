// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"

#include "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that configureCell: sets all the fields of the cell except the image
// and fetches the image through the delegate.
TEST(ContentSuggestionsItemTest, CellIsConfiguredWithoutImage) {
  // Setup.
  NSString* title = @"testTitle";
  NSString* subtitle = @"testSubtitle";
  GURL url = GURL("http://chromium.org");
  NSString* publisher = @"publisherName";
  base::Time publishTime = base::Time::Now();
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:publishTime.ToDoubleT()];
  id delegateMock = OCMProtocolMock(@protocol(SuggestedContentDelegate));
  ContentSuggestionsItem* item =
      [[ContentSuggestionsItem alloc] initWithType:0
                                             title:title
                                          subtitle:subtitle
                                               url:url];
  item.delegate = delegateMock;
  item.hasImage = YES;
  item.publisher = publisher;
  item.publishDate = publishTime;
  item.availableOffline = YES;
  OCMExpect([delegateMock loadImageForSuggestedItem:item]);
  ContentSuggestionsCell* cell = [[[item cellClass] alloc] init];
  ASSERT_EQ([ContentSuggestionsCell class], [cell class]);
  ASSERT_EQ(url, item.URL);
  ASSERT_EQ(nil, item.image);
  id cellMock = OCMPartialMock(cell);
  OCMExpect([cellMock setContentImage:item.image]);
  OCMExpect([cellMock setSubtitleText:subtitle]);
  OCMExpect([cellMock setAdditionalInformationWithPublisherName:publisher
                                                           date:date
                                            offlineAvailability:YES]);

  // Action.
  [item configureCell:cell];

  // Tests.
  EXPECT_OCMOCK_VERIFY(cellMock);
  EXPECT_EQ(title, cell.titleLabel.text);
  EXPECT_OCMOCK_VERIFY(delegateMock);
}

// Tests that configureCell: does not call the delegate if it fetched the image
// once.
TEST(ContentSuggestionsItemTest, DontFetchImageIsImageIsBeingFetched) {
  // Setup.
  NSString* title = @"testTitle";
  NSString* subtitle = @"testSubtitle";
  GURL url = GURL("http://chromium.org");
  id niceDelegateMock = OCMProtocolMock(@protocol(SuggestedContentDelegate));
  ContentSuggestionsItem* item =
      [[ContentSuggestionsItem alloc] initWithType:0
                                             title:title
                                          subtitle:subtitle
                                               url:url];
  item.delegate = niceDelegateMock;
  item.hasImage = YES;
  item.image = [[UIImage alloc] init];

  OCMExpect([niceDelegateMock loadImageForSuggestedItem:item]);
  ContentSuggestionsCell* cell = [[[item cellClass] alloc] init];
  ASSERT_NE(nil, item.image);
  [item configureCell:cell];
  ASSERT_OCMOCK_VERIFY(niceDelegateMock);

  id strictDelegateMock =
      OCMStrictProtocolMock(@protocol(SuggestedContentDelegate));
  item.delegate = strictDelegateMock;
  id cellMock = OCMPartialMock(cell);
  OCMExpect([cellMock setContentImage:item.image]);

  // Action.
  [item configureCell:cell];

  // Tests.
  EXPECT_OCMOCK_VERIFY(cellMock);
  EXPECT_EQ(title, cell.titleLabel.text);
}

// Tests that the delegate is not called when |hasImage| is set to NO. If the
// delegate is called an exception is raised.
TEST(ContentSuggestionsItemTest, NoDelegateCallWhenHasNotImage) {
  // Setup.
  NSString* title = @"testTitle";
  NSString* subtitle = @"testSubtitle";
  GURL url = GURL("http://chromium.org");
  // Strict mock. Raise exception if the load method is called.
  id delegateMock = OCMStrictProtocolMock(@protocol(SuggestedContentDelegate));
  ContentSuggestionsItem* item =
      [[ContentSuggestionsItem alloc] initWithType:0
                                             title:title
                                          subtitle:subtitle
                                               url:url];
  item.delegate = delegateMock;
  item.hasImage = NO;
  ContentSuggestionsCell* cell = [[[item cellClass] alloc] init];

  // Action.
  [item configureCell:cell];
}

}  // namespace
