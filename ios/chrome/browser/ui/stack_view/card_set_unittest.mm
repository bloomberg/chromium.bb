// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <QuartzCore/QuartzCore.h>

#include "base/strings/stringprintf.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/stack_view/card_set.h"
#import "ios/chrome/browser/ui/stack_view/card_stack_layout_manager.h"
#import "ios/chrome/browser/ui/stack_view/stack_card.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MockTabModel : NSObject<NSFastEnumeration> {
 @private
  NSMutableArray* tabs_;
  __weak id<TabModelObserver> observer_;
}

// Adds a new mock tab with the given properties.
- (void)addTabWithTitle:(NSString*)title location:(const GURL&)url;

// TabModel mocking.
- (NSUInteger)count;
- (BOOL)isOffTheRecord;
- (Tab*)currentTab;
- (NSUInteger)indexOfTab:(Tab*)tab;
- (Tab*)tabAtIndex:(NSUInteger)tabIndex;
- (void)addObserver:(id<TabModelObserver>)observer;
- (void)removeObserver:(id<TabModelObserver>)observer;
- (id<TabModelObserver>)observer;
@end

@interface CardSetTestTabMock : OCMockComplexTypeHelper
@end
@implementation CardSetTestTabMock

typedef const GURL& (^CardSetTestTabMock_url)(void);

- (const GURL&)url {
  return static_cast<CardSetTestTabMock_url>([self blockForSelector:_cmd])();
}
@end

@implementation MockTabModel

- (id)init {
  if ((self = [super init])) {
    tabs_ = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)addTabWithTitle:(NSString*)title location:(const GURL&)url {
  id tab = [[CardSetTestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
  UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
  static int sCounter = 0;
  NSString* sessionID = [NSString stringWithFormat:@"%d", sCounter++];
  BOOL no = NO;
  [[[tab stub] andReturn:dummyView] view];
  id block = [^{
    return (const GURL&)url;
  } copy];
  [tab onSelector:@selector(url) callBlockExpectation:block];

  [[tab expect] retrieveSnapshot:[OCMArg any]];
  [[[tab stub] andReturn:nil] webController];
  [[[tab stub] andReturn:nil] favicon];
  [[[tab stub] andReturnValue:OCMOCK_VALUE(no)] canGoBack];
  [[[tab stub] andReturnValue:OCMOCK_VALUE(no)] canGoForward];
  [[[tab stub] andReturn:title] title];
  [[[tab stub] andReturn:sessionID] tabId];

  [tabs_ addObject:tab];
  [observer_ tabModel:(TabModel*)self
         didInsertTab:tab
              atIndex:([tabs_ count] - 1)
         inForeground:YES];
}

- (void)removeTabAtIndex:(NSUInteger)index {
  id tab = [tabs_ objectAtIndex:index];
  [tabs_ removeObjectAtIndex:index];

  // A tab was removed at the given index.
  [observer_ tabModel:(TabModel*)self didRemoveTab:tab atIndex:index];
}

- (NSUInteger)count {
  return [tabs_ count];
}

- (BOOL)isOffTheRecord {
  return NO;
}

- (Tab*)currentTab {
  if ([tabs_ count])
    return [tabs_ objectAtIndex:0];
  return nil;
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  return [tabs_ indexOfObject:tab];
}

- (Tab*)tabAtIndex:(NSUInteger)tabIndex {
  return [tabs_ objectAtIndex:tabIndex];
}

- (void)addObserver:(id<TabModelObserver>)observer {
  observer_ = observer;
}

- (void)removeObserver:(id<TabModelObserver>)observer {
  observer_ = nil;
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(__unsafe_unretained id*)stackbuf
                                    count:(NSUInteger)len {
  return [tabs_ countByEnumeratingWithState:state objects:stackbuf count:len];
}

- (id<TabModelObserver>)observer {
  return observer_;
}

@end

#pragma mark -

namespace {

const CGFloat kViewportDimension = 100;
const CGFloat kCardDimension = 90;

CardStackLayoutManager* CreateMockCardStackLayoutManager() {
  OCMockObject* stackModelMock =
      [OCMockObject mockForClass:[CardStackLayoutManager class]];
  BOOL no = NO;
  [[[stackModelMock stub] andReturnValue:OCMOCK_VALUE(no)] layoutIsVertical];
  NSUInteger lastStartStackCardIndex = 0;
  [[[stackModelMock stub] andReturnValue:OCMOCK_VALUE(lastStartStackCardIndex)]
      lastStartStackCardIndex];
  NSArray* cards = @[];
  [[[stackModelMock stub] andReturn:cards] cards];
  return (CardStackLayoutManager*)stackModelMock;
}

class CardSetTest : public PlatformTest {
 protected:
  virtual void SetUpWithTabs(int nb_tabs) {
    tab_model_ = [[MockTabModel alloc] init];

    for (int i = 0; i < nb_tabs; ++i) {
      std::string url = base::StringPrintf("http://%d.example.com", i);
      [tab_model_ addTabWithTitle:@"NewTab" location:GURL(url)];
    }

    card_set_ = [[CardSet alloc] initWithModel:(TabModel*)tab_model_];

    display_view_ = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, kViewportDimension, kViewportDimension)];
    // Do some initial configuration of the card set.
    [card_set_ setDisplayView:display_view_];
    [card_set_ setCardSize:CGSizeMake(kCardDimension, kCardDimension)];
    [card_set_ setLayoutAxisPosition:(kViewportDimension / 2.0) isVertical:YES];
    [card_set_ configureLayoutParametersWithMargin:10];
  }

  void SetUp() override { SetUpWithTabs(2); }

  MockTabModel* tab_model_;
  UIView* display_view_;
  CardSet* card_set_;
};

TEST_F(CardSetTest, InitialLayoutState) {
  NSArray* cards = [card_set_ cards];
  EXPECT_EQ([tab_model_ count], [cards count]);

  // Tabs should be on the trailing side.
  EXPECT_EQ(CardCloseButtonSide::TRAILING, [card_set_ closeButtonSide]);

  // At least one card should be visible after layout, and cards should have
  // the right size and center.
  [card_set_ fanOutCards];
  BOOL has_visible_card = NO;
  for (StackCard* card in cards) {
    if ([card viewIsLive])
      has_visible_card = YES;
    EXPECT_FLOAT_EQ(kCardDimension, card.layout.size.width);
    EXPECT_FLOAT_EQ(kCardDimension, card.layout.size.height);
    EXPECT_FLOAT_EQ(kViewportDimension / 2.0,
                    CGRectGetMidX(LayoutRectGetRect(card.layout)));
  }
  EXPECT_TRUE(has_visible_card);
}

TEST_F(CardSetTest, HandleRotation) {
  // Rotate the card set.
  [card_set_ setLayoutAxisPosition:(kViewportDimension / 3.0) isVertical:NO];

  // Tabs should now be on the leading side.
  EXPECT_EQ(CardCloseButtonSide::LEADING, [card_set_ closeButtonSide]);
  // And the centers should be on the new axis.
  for (StackCard* card in [card_set_ cards]) {
    EXPECT_FLOAT_EQ(kViewportDimension / 3.0,
                    CGRectGetMidY(LayoutRectGetRect(card.layout)));
  }
}

TEST_F(CardSetTest, CurrentCard) {
  NSUInteger current_tab_index =
      [tab_model_ indexOfTab:[tab_model_ currentTab]];
  NSArray* cards = [card_set_ cards];
  NSUInteger current_card_index = [cards indexOfObject:[card_set_ currentCard]];
  EXPECT_EQ(current_tab_index, current_card_index);
}

TEST_F(CardSetTest, ViewClearing) {
  // Add the views.
  [card_set_ fanOutCards];
  ASSERT_GT([[display_view_ subviews] count], 0U);
  [card_set_ setDisplayView:nil];
  EXPECT_EQ(0U, [[display_view_ subviews] count]);
}

TEST_F(CardSetTest, NoticeTabAddition) {
  NSArray* cards = [card_set_ cards];
  NSUInteger initial_card_count = [cards count];

  OCMockObject* observer =
      [OCMockObject mockForProtocol:@protocol(CardSetObserver)];
  [[observer expect] cardSet:OCMOCK_ANY didAddCard:OCMOCK_ANY];
  [card_set_ setObserver:(id<CardSetObserver>)observer];

  [tab_model_ addTabWithTitle:@"NewTab"
                     location:GURL("http://www.example.com")];
  cards = [card_set_ cards];
  EXPECT_EQ(initial_card_count + 1, [cards count]);

  [card_set_ setObserver:nil];
  EXPECT_OCMOCK_VERIFY(observer);
}

TEST_F(CardSetTest, NoticeTabRemoval) {
  NSArray* cards = [card_set_ cards];
  NSUInteger initial_card_count = [cards count];
  StackCard* first_card = [cards objectAtIndex:0];

  OCMockObject* observer =
      [OCMockObject mockForProtocol:@protocol(CardSetObserver)];
  [[observer expect] cardSet:OCMOCK_ANY willRemoveCard:OCMOCK_ANY atIndex:0];
  [[observer expect] cardSet:OCMOCK_ANY didRemoveCard:OCMOCK_ANY atIndex:0];

  [card_set_ setObserver:(id<CardSetObserver>)observer];

  [tab_model_ removeTabAtIndex:0];
  cards = [card_set_ cards];
  EXPECT_EQ(initial_card_count - 1, [cards count]);
  EXPECT_NE(first_card, [cards objectAtIndex:0]);

  [card_set_ setObserver:nil];
  EXPECT_OCMOCK_VERIFY(observer);
}

TEST_F(CardSetTest, Preloading) {
  // Preloading when everything is visible should return NO.
  [card_set_ fanOutCards];
  EXPECT_FALSE([card_set_ preloadNextCard]);

  // Add a bunch of cards to ensure stacking.
  for (int i = 0; i < 20; ++i) {
    [tab_model_ addTabWithTitle:@"NewTab"
                       location:GURL("http://www.example.com")];
  }
  [card_set_ fanOutCards];

  NSUInteger loaded_cards = [[display_view_ subviews] count];
  // Now preloading should return YES, and should have added one more card to
  // the view.
  EXPECT_TRUE([card_set_ preloadNextCard]);
  EXPECT_EQ(loaded_cards + 1, [[display_view_ subviews] count]);
}

TEST_F(CardSetTest, stackIsFullyCollapsed) {
  [card_set_ setStackModelForTesting:CreateMockCardStackLayoutManager()];
  OCMockObject* stack_model_mock = (OCMockObject*)[card_set_ stackModel];
  [[stack_model_mock expect] stackIsFullyCollapsed];
  [card_set_ stackIsFullyCollapsed];
  EXPECT_OCMOCK_VERIFY(stack_model_mock);
}

TEST_F(CardSetTest, stackIsFullyFannedOut) {
  [card_set_ setStackModelForTesting:CreateMockCardStackLayoutManager()];
  OCMockObject* stack_model_mock = (OCMockObject*)[card_set_ stackModel];
  [[stack_model_mock expect] stackIsFullyFannedOut];
  [card_set_ stackIsFullyFannedOut];
  EXPECT_OCMOCK_VERIFY(stack_model_mock);
}

TEST_F(CardSetTest, stackIsFullyOverextended) {
  [card_set_ setStackModelForTesting:CreateMockCardStackLayoutManager()];
  OCMockObject* stack_model_mock = (OCMockObject*)[card_set_ stackModel];
  [[stack_model_mock expect] stackIsFullyOverextended];
  [card_set_ stackIsFullyOverextended];
  EXPECT_OCMOCK_VERIFY(stack_model_mock);
}

TEST_F(CardSetTest, isCardInStartStaggerRegion) {
  [card_set_ fanOutCards];
  NSArray* cards = [card_set_ cards];
  // First card should not be collapsed into start stagger region when stack is
  // fanned out from the first card.
  StackCard* first_card = [cards objectAtIndex:0];
  EXPECT_FALSE([card_set_ isCardInStartStaggerRegion:first_card]);
  // Add a bunch of cards to ensure stacking, and fan them out so that there
  // is a start stack.
  for (int i = 0; i < 20; ++i) {
    [tab_model_ addTabWithTitle:@"NewTab"
                       location:GURL("http://www.example.com")];
  }
  [card_set_ fanOutCardsWithStartIndex:10];
  // First card should now be collapsed into start stagger region.
  EXPECT_TRUE([card_set_ isCardInStartStaggerRegion:first_card]);
}

TEST_F(CardSetTest, isCardInEndStaggerRegion) {
  [card_set_ fanOutCards];
  NSArray* cards = [card_set_ cards];
  // Add a bunch of cards to ensure stacking, and fan them out so that there
  // is an end stack.
  for (int i = 0; i < 20; ++i) {
    [tab_model_ addTabWithTitle:@"NewTab"
                       location:GURL("http://www.example.com")];
  }
  StackCard* last_card = [cards objectAtIndex:[cards count] - 1];
  [card_set_ fanOutCards];
  // Last card should be collapsed into end region.
  EXPECT_TRUE([card_set_ isCardInEndStaggerRegion:last_card]);

  // Fan out cards from the last card.
  [card_set_ fanOutCardsWithStartIndex:[cards count] - 1];
  // Last card should not be collapsed into end region.
  EXPECT_FALSE([card_set_ isCardInEndStaggerRegion:last_card]);
}

TEST_F(CardSetTest, setTabModel) {
  SetUpWithTabs(0);
  [card_set_ setTabModel:nil];
  EXPECT_TRUE([tab_model_ observer] == nil);
  [card_set_ setTabModel:static_cast<id>(tab_model_)];
  EXPECT_NSEQ(card_set_, static_cast<id>([tab_model_ observer]));
}

}  // namespace
