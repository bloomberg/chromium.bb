// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

@interface CRWSessionController (Testing)
- (const GURL&)URLForSessionAtIndex:(NSUInteger)index;
- (const GURL&)currentURL;
@end

@implementation CRWSessionController (Testing)
- (const GURL&)URLForSessionAtIndex:(NSUInteger)index {
  CRWSessionEntry* entry =
      static_cast<CRWSessionEntry*>([self.entries objectAtIndex:index]);
  return entry.navigationItem->GetURL();
}

- (const GURL&)currentURL {
  DCHECK([self currentEntry]);
  return [self currentEntry].navigationItem->GetURL();
}
@end

namespace {

class CRWSessionControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    session_controller_.reset(
        [[CRWSessionController alloc] initWithWindowName:@"test window"
                                                openerId:@"opener"
                                             openedByDOM:NO
                                   openerNavigationIndex:0
                                            browserState:&browser_state_]);
  }

  web::Referrer MakeReferrer(const std::string& url) {
    return web::Referrer(GURL(url), web::ReferrerPolicyDefault);
  }

  web::TestWebThreadBundle thread_bundle_;
  web::TestBrowserState browser_state_;
  base::scoped_nsobject<CRWSessionController> session_controller_;
};

TEST_F(CRWSessionControllerTest, InitWithWindowName) {
  EXPECT_NSEQ(@"test window", [session_controller_ windowName]);
  EXPECT_NSEQ(@"opener", [session_controller_ openerId]);
  EXPECT_FALSE([session_controller_ isOpenedByDOM]);
  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ currentURL]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryWithCommittedEntries) {
  [session_controller_
        addPendingEntry:GURL("http://www.committed.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.committed.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ currentURL]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryOverriding) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_
        addPendingEntry:GURL("http://www.another.url.com")
               referrer:MakeReferrer("http://www.another.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.another.url.com/"),
      [session_controller_ currentURL]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryAndCommit) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryOverridingAndCommit) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_
        addPendingEntry:GURL("http://www.another.url.com")
               referrer:MakeReferrer("http://www.another.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.another.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryAndCommitMultiple) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_
        addPendingEntry:GURL("http://www.another.url.com")
               referrer:MakeReferrer("http://www.another.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(2U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.another.url.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:1U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryAndDiscard) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ discardNonCommittedEntries];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryAndDiscardAndAddAndCommit) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ discardNonCommittedEntries];

  [session_controller_
        addPendingEntry:GURL("http://www.another.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.another.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, AddPendingEntryAndCommitAndAddAndDiscard) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_
        addPendingEntry:GURL("http://www.another.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ discardNonCommittedEntries];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest,
       CommitPendingEntryWithoutPendingOrCommittedEntry) {
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest,
       CommitPendingEntryWithoutPendingEntryWithCommittedEntry) {
  // Setup committed entry
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  // Commit pending entry when there is no such one
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest,
       DiscardPendingEntryWithoutPendingOrCommittedEntry) {
  [session_controller_ discardNonCommittedEntries];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest,
       DiscardPendingEntryWithoutPendingEntryWithCommittedEntry) {
  // Setup committed entry
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  // Discard noncommitted entries when there is no such one
  [session_controller_ discardNonCommittedEntries];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, UpdatePendingEntryWithoutPendingEntry) {
  [session_controller_
       updatePendingEntry:GURL("http://www.another.url.com")];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, UpdatePendingEntryWithPendingEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_
       updatePendingEntry:GURL("http://www.another.url.com")];

  EXPECT_EQ(
      GURL("http://www.another.url.com/"),
      [session_controller_ currentURL]);
}

TEST_F(CRWSessionControllerTest,
       UpdatePendingEntryWithPendingEntryAlreadyCommited) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
       updatePendingEntry:GURL("http://www.another.url.com")];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoBackWithoutCommitedEntry) {
  [session_controller_ goBack];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoBackWithSingleCommitedEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoBackFromTheEnd) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];

  EXPECT_EQ(2U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url2.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoBackFromTheBeginning) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];
  [session_controller_ goBack];

  EXPECT_EQ(2U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url2.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoBackFromTheMiddle) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url3.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url4.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];
  [session_controller_ goBack];

  EXPECT_EQ(4U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url2.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      GURL("http://www.url3.com/"),
      [session_controller_ URLForSessionAtIndex:2U]);
  EXPECT_EQ(
      GURL("http://www.url4.com/"),
      [session_controller_ URLForSessionAtIndex:3U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:1U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoBackAndRemove) {
  [session_controller_
   addPendingEntry:GURL("http://www.url.com")
   referrer:MakeReferrer("http://www.referer.com")
   transition:ui::PAGE_TRANSITION_TYPED
   rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
   addPendingEntry:GURL("http://www.url2.com")
   referrer:MakeReferrer("http://www.referer.com")
   transition:ui::PAGE_TRANSITION_TYPED
   rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];
  [session_controller_ removeEntryAtIndex:1];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
            GURL("http://www.url.com/"),
            [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
            [[session_controller_ entries] objectAtIndex:0U],
            [session_controller_ currentEntry]);
  EXPECT_EQ([session_controller_ currentEntry],
            [session_controller_ previousEntry]);
}

TEST_F(CRWSessionControllerTest, GoForwardWithoutCommitedEntry) {
  [session_controller_ goForward];

  EXPECT_EQ(0U, [[session_controller_ entries] count]);
  EXPECT_EQ(nil, [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoForwardWithSingleCommitedEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goForward];

  EXPECT_EQ(1U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:0U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoForewardFromTheEnd) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goForward];

  EXPECT_EQ(2U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url2.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:1U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoForewardFromTheBeginning) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];
  [session_controller_ goForward];

  EXPECT_EQ(2U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url2.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:1U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, GoForwardFromTheMiddle) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url3.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url4.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  [session_controller_ goBack];
  [session_controller_ goBack];
  [session_controller_ goForward];

  EXPECT_EQ(4U, [[session_controller_ entries] count]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ URLForSessionAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url2.com/"),
      [session_controller_ URLForSessionAtIndex:1U]);
  EXPECT_EQ(
      GURL("http://www.url3.com/"),
      [session_controller_ URLForSessionAtIndex:2U]);
  EXPECT_EQ(
      GURL("http://www.url4.com/"),
      [session_controller_ URLForSessionAtIndex:3U]);
  EXPECT_EQ(
      [[session_controller_ entries] objectAtIndex:2U],
      [session_controller_ currentEntry]);
}

TEST_F(CRWSessionControllerTest, CanGoBackWithoutCommitedEntry) {
  EXPECT_FALSE([session_controller_ canGoBack]);
}

TEST_F(CRWSessionControllerTest, CanGoBackWithSingleCommitedEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_FALSE([session_controller_ canGoBack]);
}

TEST_F(CRWSessionControllerTest, CanGoBackWithMultipleCommitedEntries) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url1.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_TRUE([session_controller_ canGoBack]);

  [session_controller_ goBack];
  EXPECT_TRUE([session_controller_ canGoBack]);

  [session_controller_ goBack];
  EXPECT_FALSE([session_controller_ canGoBack]);

  [session_controller_ goBack];
  EXPECT_FALSE([session_controller_ canGoBack]);

  [session_controller_ goForward];
  EXPECT_TRUE([session_controller_ canGoBack]);
}

TEST_F(CRWSessionControllerTest, CanGoForwardWithoutCommitedEntry) {
  EXPECT_FALSE([session_controller_ canGoBack]);
}

TEST_F(CRWSessionControllerTest, CanGoForwardWithSingleCommitedEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_FALSE([session_controller_ canGoBack]);
}

TEST_F(CRWSessionControllerTest, CanGoForwardWithMultipleCommitedEntries) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url1.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_FALSE([session_controller_ canGoForward]);

  [session_controller_ goBack];
  EXPECT_TRUE([session_controller_ canGoForward]);

  [session_controller_ goBack];
  EXPECT_TRUE([session_controller_ canGoForward]);

  [session_controller_ goForward];
  EXPECT_TRUE([session_controller_ canGoForward]);

  [session_controller_ goForward];
  EXPECT_FALSE([session_controller_ canGoForward]);
}

// Helper to create a NavigationItem. Caller is responsible for freeing
// the memory.
web::NavigationItem* CreateNavigationItem(const std::string& url,
                                          const std::string& referrer,
                                          NSString* title) {
  web::Referrer referrer_object(GURL(referrer),
                                web::ReferrerPolicyDefault);
  web::NavigationItemImpl* navigation_item = new web::NavigationItemImpl();
  navigation_item->SetURL(GURL(url));
  navigation_item->SetReferrer(referrer_object);
  navigation_item->SetTitle(base::SysNSStringToUTF16(title));
  navigation_item->SetTransitionType(ui::PAGE_TRANSITION_TYPED);

  return navigation_item;
}

TEST_F(CRWSessionControllerTest, CreateWithEmptyNavigations) {
  ScopedVector<web::NavigationItem> items;
  base::scoped_nsobject<CRWSessionController> controller(
      [[CRWSessionController alloc] initWithNavigationItems:items.Pass()
                                               currentIndex:0
                                               browserState:&browser_state_]);
  EXPECT_EQ(controller.get().entries.count, 0U);
  EXPECT_EQ(controller.get().currentNavigationIndex, -1);
  EXPECT_EQ(controller.get().previousNavigationIndex, -1);
  EXPECT_FALSE(controller.get().currentEntry);
}

TEST_F(CRWSessionControllerTest, CreateWithNavList) {
  ScopedVector<web::NavigationItem> items;
  items.push_back(CreateNavigationItem("http://www.google.com",
                                       "http://www.referrer.com", @"Google"));
  items.push_back(CreateNavigationItem("http://www.yahoo.com",
                                       "http://www.google.com", @"Yahoo"));
  items.push_back(CreateNavigationItem("http://www.espn.com",
                                       "http://www.nothing.com", @"ESPN"));
  base::scoped_nsobject<CRWSessionController> controller(
      [[CRWSessionController alloc] initWithNavigationItems:items.Pass()
                                               currentIndex:1
                                               browserState:&browser_state_]);

  EXPECT_EQ(controller.get().entries.count, 3U);
  EXPECT_EQ(controller.get().currentNavigationIndex, 1);
  EXPECT_EQ(controller.get().previousNavigationIndex, -1);
  // Sanity check the current entry, the CRWSessionEntry unit test will ensure
  // the entire object is created properly.
  CRWSessionEntry* current_entry = controller.get().currentEntry;
  EXPECT_EQ(current_entry.navigationItem->GetURL(),
            GURL("http://www.yahoo.com"));
  EXPECT_EQ([[controller openerId] length], 0UL);
}

TEST_F(CRWSessionControllerTest, PreviousNavigationEntry) {
  [session_controller_
        addPendingEntry:GURL("http://www.url.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url1.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_
        addPendingEntry:GURL("http://www.url2.com")
               referrer:MakeReferrer("http://www.referer.com")
             transition:ui::PAGE_TRANSITION_TYPED
      rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(session_controller_.get().previousNavigationIndex, 1);

  [session_controller_ goBack];
  EXPECT_EQ(session_controller_.get().previousNavigationIndex, 2);

  [session_controller_ goBack];
  EXPECT_EQ(session_controller_.get().previousNavigationIndex, 1);

  [session_controller_ goForward];
  EXPECT_EQ(session_controller_.get().previousNavigationIndex, 0);

  [session_controller_ goForward];
  EXPECT_EQ(session_controller_.get().previousNavigationIndex, 1);
}

TEST_F(CRWSessionControllerTest, PushNewEntry) {
  ScopedVector<web::NavigationItem> items;
  items.push_back(CreateNavigationItem("http://www.firstpage.com",
                                       "http://www.starturl.com", @"First"));
  items.push_back(CreateNavigationItem("http://www.secondpage.com",
                                       "http://www.firstpage.com", @"Second"));
  items.push_back(CreateNavigationItem("http://www.thirdpage.com",
                                       "http://www.secondpage.com", @"Third"));
  base::scoped_nsobject<CRWSessionController> controller(
      [[CRWSessionController alloc] initWithNavigationItems:items.Pass()
                                               currentIndex:0
                                               browserState:&browser_state_]);

  GURL pushPageGurl1("http://www.firstpage.com/#push1");
  NSString* stateObject1 = @"{'foo': 1}";
  [controller pushNewEntryWithURL:pushPageGurl1
                      stateObject:stateObject1
                       transition:ui::PAGE_TRANSITION_LINK];
  CRWSessionEntry* pushedEntry = [controller currentEntry];
  web::NavigationItemImpl* pushedItem = pushedEntry.navigationItemImpl;
  NSUInteger expectedCount = 2;
  EXPECT_EQ(expectedCount, controller.get().entries.count);
  EXPECT_EQ(pushPageGurl1, pushedEntry.navigationItem->GetURL());
  EXPECT_TRUE(pushedItem->IsCreatedFromPushState());
  EXPECT_NSEQ(stateObject1, pushedItem->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://www.firstpage.com/"),
            pushedEntry.navigationItem->GetReferrer().url);

  // Add another new entry and check size and fields again.
  GURL pushPageGurl2("http://www.firstpage.com/push2");
  [controller pushNewEntryWithURL:pushPageGurl2
                      stateObject:nil
                       transition:ui::PAGE_TRANSITION_LINK];
  pushedEntry = [controller currentEntry];
  pushedItem = pushedEntry.navigationItemImpl;
  expectedCount = 3;
  EXPECT_EQ(expectedCount, controller.get().entries.count);
  EXPECT_EQ(pushPageGurl2, pushedEntry.navigationItem->GetURL());
  EXPECT_TRUE(pushedItem->IsCreatedFromPushState());
  EXPECT_EQ(nil, pushedItem->GetSerializedStateObject());
  EXPECT_EQ(pushPageGurl1, pushedEntry.navigationItem->GetReferrer().url);
}

TEST_F(CRWSessionControllerTest, IsPushStateNavigation) {
  ScopedVector<web::NavigationItem> items;
  items.push_back(
      CreateNavigationItem("http://foo.com", "http://google.com", @"First"));
  // Push state navigation.
  items.push_back(
      CreateNavigationItem("http://foo.com#bar", "http://foo.com", @"Second"));
  items.push_back(CreateNavigationItem("http://google.com",
                                       "http://foo.com#bar", @"Third"));
  items.push_back(
      CreateNavigationItem("http://foo.com", "http://google.com", @"Fourth"));
  // Push state navigation.
  items.push_back(
      CreateNavigationItem("http://foo.com/bar", "http://foo.com", @"Fifth"));
  // Push state navigation.
  items.push_back(CreateNavigationItem("http://foo.com/bar#bar",
                                       "http://foo.com/bar", @"Sixth"));
  base::scoped_nsobject<CRWSessionController> controller(
      [[CRWSessionController alloc] initWithNavigationItems:items.Pass()
                                               currentIndex:0
                                               browserState:&browser_state_]);
  CRWSessionEntry* entry0 = [controller.get().entries objectAtIndex:0];
  CRWSessionEntry* entry1 = [controller.get().entries objectAtIndex:1];
  CRWSessionEntry* entry2 = [controller.get().entries objectAtIndex:2];
  CRWSessionEntry* entry3 = [controller.get().entries objectAtIndex:3];
  CRWSessionEntry* entry4 = [controller.get().entries objectAtIndex:4];
  CRWSessionEntry* entry5 = [controller.get().entries objectAtIndex:5];
  entry1.navigationItemImpl->SetIsCreatedFromPushState(true);
  entry4.navigationItemImpl->SetIsCreatedFromPushState(true);
  entry5.navigationItemImpl->SetIsCreatedFromPushState(true);

  EXPECT_TRUE(
      [controller isPushStateNavigationBetweenEntry:entry0 andEntry:entry1]);
  EXPECT_TRUE(
      [controller isPushStateNavigationBetweenEntry:entry5 andEntry:entry3]);
  EXPECT_TRUE(
      [controller isPushStateNavigationBetweenEntry:entry4 andEntry:entry3]);
  EXPECT_FALSE(
      [controller isPushStateNavigationBetweenEntry:entry1 andEntry:entry2]);
  EXPECT_FALSE(
      [controller isPushStateNavigationBetweenEntry:entry0 andEntry:entry5]);
  EXPECT_FALSE(
      [controller isPushStateNavigationBetweenEntry:entry2 andEntry:entry4]);
}

TEST_F(CRWSessionControllerTest, UpdateCurrentEntry) {
  ScopedVector<web::NavigationItem> items;
  items.push_back(CreateNavigationItem("http://www.firstpage.com",
                                       "http://www.starturl.com", @"First"));
  items.push_back(CreateNavigationItem("http://www.secondpage.com",
                                       "http://www.firstpage.com", @"Second"));
  items.push_back(CreateNavigationItem("http://www.thirdpage.com",
                                       "http://www.secondpage.com", @"Third"));
  base::scoped_nsobject<CRWSessionController> controller(
      [[CRWSessionController alloc] initWithNavigationItems:items.Pass()
                                               currentIndex:0
                                               browserState:&browser_state_]);

  GURL replacePageGurl1("http://www.firstpage.com/#replace1");
  NSString* stateObject1 = @"{'foo': 1}";

  // Replace current entry and check the size of history and fields of the
  // modified entry.
  [controller updateCurrentEntryWithURL:replacePageGurl1
                            stateObject:stateObject1];
  CRWSessionEntry* replacedEntry = [controller currentEntry];
  web::NavigationItemImpl* replacedItem = replacedEntry.navigationItemImpl;
  NSUInteger expectedCount = 3;
  EXPECT_EQ(expectedCount, controller.get().entries.count);
  EXPECT_EQ(replacePageGurl1, replacedEntry.navigationItem->GetURL());
  EXPECT_FALSE(replacedItem->IsCreatedFromPushState());
  EXPECT_NSEQ(stateObject1, replacedItem->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://www.starturl.com/"),
            replacedEntry.navigationItem->GetReferrer().url);

  // Replace current entry and check size and fields again.
  GURL replacePageGurl2("http://www.firstpage.com/#replace2");
  [controller.get() updateCurrentEntryWithURL:replacePageGurl2 stateObject:nil];
  replacedEntry = [controller currentEntry];
  replacedItem = replacedEntry.navigationItemImpl;
  EXPECT_EQ(expectedCount, controller.get().entries.count);
  EXPECT_EQ(replacePageGurl2, replacedEntry.navigationItem->GetURL());
  EXPECT_FALSE(replacedItem->IsCreatedFromPushState());
  EXPECT_NSEQ(nil, replacedItem->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://www.starturl.com/"),
            replacedEntry.navigationItem->GetReferrer().url);
}

TEST_F(CRWSessionControllerTest, TestBackwardForwardEntries) {
  [session_controller_ addPendingEntry:GURL("http://www.example.com/0")
                             referrer:MakeReferrer("http://www.example.com/a")
                           transition:ui::PAGE_TRANSITION_LINK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_ addPendingEntry:GURL("http://www.example.com/1")
                             referrer:MakeReferrer("http://www.example.com/b")
                           transition:ui::PAGE_TRANSITION_LINK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_ addPendingEntry:GURL("http://www.example.com/redirect")
                             referrer:MakeReferrer("http://www.example.com/r")
                           transition:ui::PAGE_TRANSITION_IS_REDIRECT_MASK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_ addPendingEntry:GURL("http://www.example.com/2")
                             referrer:MakeReferrer("http://www.example.com/c")
                           transition:ui::PAGE_TRANSITION_LINK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];

  EXPECT_EQ(3, session_controller_.get().currentNavigationIndex);
  NSArray* backEntries = [session_controller_ backwardEntries];
  EXPECT_EQ(2U, [backEntries count]);
  EXPECT_EQ(0U, [[session_controller_ forwardEntries] count]);
  EXPECT_EQ("http://www.example.com/1",
            [[backEntries objectAtIndex:0] navigationItem]->GetURL().spec());

  [session_controller_ goBack];
  EXPECT_EQ(1U, [[session_controller_ backwardEntries] count]);
  EXPECT_EQ(1U, [[session_controller_ forwardEntries] count]);

  [session_controller_ goBack];
  NSArray* forwardEntries = [session_controller_ forwardEntries];
  EXPECT_EQ(0U, [[session_controller_ backwardEntries] count]);
  EXPECT_EQ(2U, [forwardEntries count]);
  EXPECT_EQ("http://www.example.com/2",
            [[forwardEntries objectAtIndex:1] navigationItem]->GetURL().spec());
}

TEST_F(CRWSessionControllerTest, GoToEntry) {
  [session_controller_ addPendingEntry:GURL("http://www.example.com/0")
                             referrer:MakeReferrer("http://www.example.com/a")
                           transition:ui::PAGE_TRANSITION_LINK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_ addPendingEntry:GURL("http://www.example.com/1")
                             referrer:MakeReferrer("http://www.example.com/b")
                           transition:ui::PAGE_TRANSITION_LINK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_ addPendingEntry:GURL("http://www.example.com/redirect")
                             referrer:MakeReferrer("http://www.example.com/r")
                           transition:ui::PAGE_TRANSITION_IS_REDIRECT_MASK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  [session_controller_ addPendingEntry:GURL("http://www.example.com/2")
                             referrer:MakeReferrer("http://www.example.com/c")
                           transition:ui::PAGE_TRANSITION_LINK
                    rendererInitiated:NO];
  [session_controller_ commitPendingEntry];
  EXPECT_EQ(3, session_controller_.get().currentNavigationIndex);

  CRWSessionEntry* entry1 = [session_controller_.get().entries objectAtIndex:1];
  [session_controller_ goToEntry:entry1];
  EXPECT_EQ(1, session_controller_.get().currentNavigationIndex);

  // Remove an entry and attempt to go it. Ensure it outlives the removal.
  base::scoped_nsobject<CRWSessionEntry> entry3(
      [[session_controller_.get().entries objectAtIndex:3] retain]);
  [session_controller_ removeEntryAtIndex:3];
  [session_controller_ goToEntry:entry3];
  EXPECT_EQ(1, session_controller_.get().currentNavigationIndex);
}

}  // anonymous namespace
