// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/mock_browsing_data_appcache_helper.h"
#include "chrome/browser/mock_browsing_data_database_helper.h"
#include "chrome/browser/mock_browsing_data_indexed_db_helper.h"
#include "chrome/browser/mock_browsing_data_local_storage_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/clear_browsing_data_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/options/cookies_window_controller.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/tree_model.h"

// Used to test FindCocoaNode. This only sets the title and node, without
// initializing any other members.
@interface FakeCocoaCookieTreeNode : CocoaCookieTreeNode {
  ui::TreeModelNode* testNode_;
}
- (id)initWithTreeNode:(ui::TreeModelNode*)node;
@end
@implementation FakeCocoaCookieTreeNode
- (id)initWithTreeNode:(ui::TreeModelNode*)node {
  if ((self = [super init])) {
    testNode_ = node;
    children_.reset([[NSMutableArray alloc] init]);
  }
  return self;
}
- (ui::TreeModelNode*)treeNode {
  return testNode_;
}
@end

namespace {

class CookiesWindowControllerTest : public CocoaTest {
 public:

  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    profile->CreateRequestContext();
    database_helper_ = new MockBrowsingDataDatabaseHelper(profile);
    local_storage_helper_ = new MockBrowsingDataLocalStorageHelper(profile);
    appcache_helper_ = new MockBrowsingDataAppCacheHelper(profile);
    indexed_db_helper_ = new MockBrowsingDataIndexedDBHelper(profile);
    controller_.reset(
        [[CookiesWindowController alloc] initWithProfile:profile
                                          databaseHelper:database_helper_
                                           storageHelper:local_storage_helper_
                                          appcacheHelper:appcache_helper_
                                         indexedDBHelper:indexed_db_helper_]
    );
  }

  virtual void TearDown() {
    CocoaTest::TearDown();
  }

  CocoaCookieTreeNode* CocoaNodeFromTreeNode(ui::TreeModelNode* node) {
    return [controller_ modelObserver]->CocoaNodeFromTreeNode(node);
  }

  CocoaCookieTreeNode* FindCocoaNode(ui::TreeModelNode* node,
                                     CocoaCookieTreeNode* start) {
    return [controller_ modelObserver]->FindCocoaNode(node, start);
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_nsobject<CookiesWindowController> controller_;
  MockBrowsingDataDatabaseHelper* database_helper_;
  MockBrowsingDataLocalStorageHelper* local_storage_helper_;
  MockBrowsingDataAppCacheHelper* appcache_helper_;
  MockBrowsingDataIndexedDBHelper* indexed_db_helper_;
};

TEST_F(CookiesWindowControllerTest, Construction) {
  std::vector<SkBitmap> skia_icons;
  [controller_ treeModel]->GetIcons(&skia_icons);

  EXPECT_EQ([[controller_ icons] count], skia_icons.size() + 1U);
}

TEST_F(CookiesWindowControllerTest, FindCocoaNodeRoot) {
  scoped_ptr< ui::TreeNodeWithValue<int> > search(
      new ui::TreeNodeWithValue<int>(42));
  scoped_nsobject<FakeCocoaCookieTreeNode> node(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:search.get()]);
  EXPECT_EQ(node.get(), FindCocoaNode(search.get(), node.get()));
}

TEST_F(CookiesWindowControllerTest, FindCocoaNodeImmediateChild) {
  scoped_ptr< ui::TreeNodeWithValue<int> > parent(
      new ui::TreeNodeWithValue<int>(100));
  scoped_ptr< ui::TreeNodeWithValue<int> > child1(
      new ui::TreeNodeWithValue<int>(10));
  scoped_ptr< ui::TreeNodeWithValue<int> > child2(
      new ui::TreeNodeWithValue<int>(20));
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaParent(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:parent.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild1(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child1.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild2(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child2.get()]);
  [[cocoaParent mutableChildren] addObject:cocoaChild1.get()];
  [[cocoaParent mutableChildren] addObject:cocoaChild2.get()];

  EXPECT_EQ(cocoaChild2.get(), FindCocoaNode(child2.get(), cocoaParent.get()));
}

TEST_F(CookiesWindowControllerTest, FindCocoaNodeRecursive) {
  scoped_ptr< ui::TreeNodeWithValue<int> > parent(
      new ui::TreeNodeWithValue<int>(100));
  scoped_ptr< ui::TreeNodeWithValue<int> > child1(
      new ui::TreeNodeWithValue<int>(10));
  scoped_ptr< ui::TreeNodeWithValue<int> > child2(
      new ui::TreeNodeWithValue<int>(20));
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaParent(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:parent.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild1(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child1.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild2(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child2.get()]);
  [[cocoaParent mutableChildren] addObject:cocoaChild1.get()];
  [[cocoaChild1 mutableChildren] addObject:cocoaChild2.get()];

  EXPECT_EQ(cocoaChild2.get(), FindCocoaNode(child2.get(), cocoaParent.get()));
}

TEST_F(CookiesWindowControllerTest, CocoaNodeFromTreeNodeCookie) {
  net::CookieMonster* cm = browser_helper_.profile()->GetCookieMonster();
  cm->SetCookie(GURL("http://foo.com"), "A=B");
  CookiesTreeModel model(cm, database_helper_, local_storage_helper_, nil, nil,
      nil);

  // Root --> foo.com --> Cookies --> A. Create node for 'A'.
  ui::TreeModelNode* node =
      model.GetRoot()->GetChild(0)->GetChild(0)->GetChild(0);
  CocoaCookieTreeNode* cookie = CocoaNodeFromTreeNode(node);

  CocoaCookieDetails* details = [cookie details];
  EXPECT_NSEQ(@"B", [details content]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_COOKIE_EXPIRES_SESSION),
              [details expires]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_COOKIE_SENDFOR_ANY),
              [details sendFor]);
  EXPECT_NSEQ(@"A", [cookie title]);
  EXPECT_NSEQ(@"A", [details name]);
  EXPECT_NSEQ(@"/", [details path]);
  EXPECT_EQ(0U, [[cookie children] count]);
  EXPECT_TRUE([details created]);
  EXPECT_TRUE([cookie isLeaf]);
  EXPECT_EQ(node, [cookie treeNode]);
}

TEST_F(CookiesWindowControllerTest, CocoaNodeFromTreeNodeRecursive) {
  net::CookieMonster* cm = browser_helper_.profile()->GetCookieMonster();
  cm->SetCookie(GURL("http://foo.com"), "A=B");
  CookiesTreeModel model(cm, database_helper_, local_storage_helper_, nil, nil,
      nil);

  // Root --> foo.com --> Cookies --> A. Create node for 'foo.com'.
  CookieTreeNode* node = model.GetRoot()->GetChild(0);
  CocoaCookieTreeNode* domain = CocoaNodeFromTreeNode(node);
  CocoaCookieTreeNode* cookies = [[domain children] objectAtIndex:0];
  CocoaCookieTreeNode* cookie = [[cookies children] objectAtIndex:0];

  // Test domain-level node.
  EXPECT_NSEQ(@"foo.com", [domain title]);

  EXPECT_FALSE([domain isLeaf]);
  EXPECT_EQ(1U, [[domain children] count]);
  EXPECT_EQ(node, [domain treeNode]);

  // Test "Cookies" folder node.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_COOKIES), [cookies title]);
  EXPECT_FALSE([cookies isLeaf]);
  EXPECT_EQ(1U, [[cookies children] count]);
  EXPECT_EQ(node->GetChild(0), [cookies treeNode]);

  // Test cookie node. This is the same as CocoaNodeFromTreeNodeCookie.
  CocoaCookieDetails* details = [cookie details];
  EXPECT_NSEQ(@"B", [details content]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_COOKIE_EXPIRES_SESSION),
              [details expires]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_COOKIE_SENDFOR_ANY),
              [details sendFor]);
  EXPECT_NSEQ(@"A", [cookie title]);
  EXPECT_NSEQ(@"A", [details name]);
  EXPECT_NSEQ(@"/", [details path]);
  EXPECT_NSEQ(@"foo.com", [details domain]);
  EXPECT_EQ(0U, [[cookie children] count]);
  EXPECT_TRUE([details created]);
  EXPECT_TRUE([cookie isLeaf]);
  EXPECT_EQ(node->GetChild(0)->GetChild(0), [cookie treeNode]);
}

TEST_F(CookiesWindowControllerTest, TreeNodesAdded) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);

  // Root --> foo.com --> Cookies.
  NSMutableArray* cocoa_children =
      [[[[[[controller_ cocoaTreeModel] children] objectAtIndex:0]
          children] objectAtIndex:0] mutableChildren];
  EXPECT_EQ(1U, [cocoa_children count]);

  // Create some cookies.
  cm->SetCookie(url, "C=D");
  cm->SetCookie(url, "E=F");

  net::CookieList list = cm->GetAllCookies();
  CookiesTreeModel* model = [controller_ treeModel];
  // Root --> foo.com --> Cookies.
  CookieTreeNode* parent = model->GetRoot()->GetChild(0)->GetChild(0);

  ASSERT_EQ(3U, list.size());

  // Add the cookie nodes.
  CookieTreeCookieNode* cnode = new CookieTreeCookieNode(&list[1]);
  parent->Add(1, cnode);  // |parent| takes ownership.
  cnode = new CookieTreeCookieNode(&list[2]);
  parent->Add(2, cnode);

  // Manually notify the observer.
  [controller_ modelObserver]->TreeNodesAdded(model, parent, 1, 2);

  // Check that we have created 2 more Cocoa nodes.
  EXPECT_EQ(3U, [cocoa_children count]);
}

TEST_F(CookiesWindowControllerTest, TreeNodesRemoved) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");
  cm->SetCookie(url, "C=D");
  cm->SetCookie(url, "E=F");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);

  // Root --> foo.com --> Cookies.
  NSMutableArray* cocoa_children =
      [[[[[[controller_ cocoaTreeModel] children] objectAtIndex:0]
          children] objectAtIndex:0] mutableChildren];
  EXPECT_EQ(3U, [cocoa_children count]);

  CookiesTreeModel* model = [controller_ treeModel];
  // Root --> foo.com --> Cookies.
  CookieTreeNode* parent = model->GetRoot()->GetChild(0)->GetChild(0);

  // Pretend to remove the nodes.
  [controller_ modelObserver]->TreeNodesRemoved(model, parent, 1, 2);

  EXPECT_EQ(1U, [cocoa_children count]);

  NSString* title = [[[cocoa_children objectAtIndex:0] details] name];
  EXPECT_NSEQ(@"A", title);
}

TEST_F(CookiesWindowControllerTest, TreeNodeChanged) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);

  CookiesTreeModel* model = [controller_ treeModel];
  // Root --> foo.com --> Cookies.
  CookieTreeNode* node = model->GetRoot()->GetChild(0)->GetChild(0);

  // Root --> foo.com --> Cookies.
  CocoaCookieTreeNode* cocoa_node =
      [[[[[controller_ cocoaTreeModel] children] objectAtIndex:0]
          children] objectAtIndex:0];

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_COOKIES),
              [cocoa_node title]);

  // Fake update the cookie folder's title. This would never happen in reality,
  // but it tests the code path that ultimately calls CocoaNodeFromTreeNode,
  // which is tested elsewhere.
  node->SetTitle(ASCIIToUTF16("Silly Change"));
  [controller_ modelObserver]->TreeNodeChanged(model, node);

  EXPECT_NSEQ(@"Silly Change", [cocoa_node title]);
}

TEST_F(CookiesWindowControllerTest, DeleteCookie) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");
  cm->SetCookie(url, "C=D");
  cm->SetCookie(GURL("http://google.com"), "E=F");

  // This will clean itself up when we call |-closeSheet:|. If we reset the
  // scoper, we'd get a double-free.
  CookiesWindowController* controller =
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_];
  [controller attachSheetTo:test_window()];
  NSTreeController* treeController = [controller treeController];

  // Select cookie A.
  NSUInteger pathA[3] = {0, 0, 0};
  NSIndexPath* indexPath = [NSIndexPath indexPathWithIndexes:pathA length:3];
  [treeController setSelectionIndexPath:indexPath];

  // Press the "Delete" button.
  [controller deleteCookie:nil];

  // Root --> foo.com --> Cookies.
  NSArray* cookies = [[[[[[controller cocoaTreeModel] children]
      objectAtIndex:0] children] objectAtIndex:0] children];
  EXPECT_EQ(1U, [cookies count]);
  EXPECT_NSEQ(@"C", [[cookies lastObject] title]);
  EXPECT_NSEQ(indexPath, [treeController selectionIndexPath]);

  // Select cookie E.
  NSUInteger pathE[3] = {1, 0, 0};
  indexPath = [NSIndexPath indexPathWithIndexes:pathE length:3];
  [treeController setSelectionIndexPath:indexPath];

  // Perform delete.
  [controller deleteCookie:nil];

  // Make sure that both the domain level node and the Cookies folder node got
  // deleted because there was only one leaf node.
  EXPECT_EQ(1U, [[[controller cocoaTreeModel] children] count]);

  // Select cookie C.
  NSUInteger pathC[3] = {0, 0, 0};
  indexPath = [NSIndexPath indexPathWithIndexes:pathC length:3];
  [treeController setSelectionIndexPath:indexPath];

  // Perform delete.
  [controller deleteCookie:nil];

  // Make sure the world didn't explode and that there's nothing in the tree.
  EXPECT_EQ(0U, [[[controller cocoaTreeModel] children] count]);

  [controller closeSheet:nil];
}

TEST_F(CookiesWindowControllerTest, DidExpandItem) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");
  cm->SetCookie(url, "C=D");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);

  // Root --> foo.com.
  CocoaCookieTreeNode* foo =
      [[[controller_ cocoaTreeModel] children] objectAtIndex:0];

  // Create the objects we are going to be testing with.
  id outlineView = [OCMockObject mockForClass:[NSOutlineView class]];
  id treeNode = [OCMockObject mockForClass:[NSTreeNode class]];
  NSTreeNode* childTreeNode =
      [NSTreeNode treeNodeWithRepresentedObject:[[foo children] lastObject]];
  NSArray* fakeChildren = [NSArray arrayWithObject:childTreeNode];

  // Set up the mock object.
  [[[treeNode stub] andReturn:foo] representedObject];
  [[[treeNode stub] andReturn:fakeChildren] childNodes];

  // Create a fake "ItemDidExpand" notification.
  NSDictionary* userInfo = [NSDictionary dictionaryWithObject:treeNode
                                                       forKey:@"NSObject"];
  NSNotification* notif =
      [NSNotification notificationWithName:@"ItemDidExpandNotification"
                                    object:outlineView
                                  userInfo:userInfo];

  // Make sure we work correctly.
  [[outlineView expect] expandItem:childTreeNode];
  [controller_ outlineViewItemDidExpand:notif];
  [outlineView verify];
}

TEST_F(CookiesWindowControllerTest, ClearBrowsingData) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");
  cm->SetCookie(url, "C=D");
  cm->SetCookie(url, "E=F");

  id mock = [OCMockObject partialMockForObject:controller_.get()];
  [[mock expect] loadTreeModelFromProfile];

  NSNumber* mask =
      [NSNumber numberWithInt:BrowsingDataRemover::REMOVE_COOKIES];
  NSDictionary* userInfo =
      [NSDictionary dictionaryWithObject:mask
                                forKey:kClearBrowsingDataControllerRemoveMask];
  NSNotification* notif =
    [NSNotification notificationWithName:kClearBrowsingDataControllerDidDelete
                                  object:nil
                                userInfo:userInfo];
  [controller_ clearBrowsingDataNotification:notif];

  [mock verify];
}

// This test has been flaky under Valgrind and turns the bot red since r38504.
// Under Mac Tests 10.5, it occasionally reports:
//   malloc: *** error for object 0x31e0468: Non-aligned pointer being freed
//   *** set a breakpoint in malloc_error_break to debug
// Attempts to reproduce locally were not successful. This code is likely
// changing in the future, so it's marked flaky for now. http://crbug.com/35327
TEST_F(CookiesWindowControllerTest, FLAKY_RemoveButtonEnabled) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");
  cm->SetCookie(url, "C=D");

  // This will clean itself up when we call |-closeSheet:|. If we reset the
  // scoper, we'd get a double-free.
  database_helper_ = new MockBrowsingDataDatabaseHelper(profile);
  local_storage_helper_ = new MockBrowsingDataLocalStorageHelper(profile);
  local_storage_helper_->AddLocalStorageSamples();
  CookiesWindowController* controller =
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_];
  local_storage_helper_->Notify();
  [controller attachSheetTo:test_window()];

  // Nothing should be selected right now.
  EXPECT_FALSE([controller removeButtonEnabled]);

  {
    // Pretend to select cookie A.
    NSUInteger path[3] = {0, 0, 0};
    NSIndexPath* indexPath = [NSIndexPath indexPathWithIndexes:path length:3];
    [[controller treeController] setSelectionIndexPath:indexPath];
    [controller outlineViewSelectionDidChange:nil];
    EXPECT_TRUE([controller removeButtonEnabled]);
  }

  {
    // Pretend to select cookie C.
    NSUInteger path[3] = {0, 0, 1};
    NSIndexPath* indexPath = [NSIndexPath indexPathWithIndexes:path length:3];
    [[controller treeController] setSelectionIndexPath:indexPath];
    [controller outlineViewSelectionDidChange:nil];
    EXPECT_TRUE([controller removeButtonEnabled]);
  }

  {
    // Select a local storage node.
    NSUInteger path[3] = {2, 0, 0};
    NSIndexPath* indexPath = [NSIndexPath indexPathWithIndexes:path length:3];
    [[controller treeController] setSelectionIndexPath:indexPath];
    [controller outlineViewSelectionDidChange:nil];
    EXPECT_TRUE([controller removeButtonEnabled]);
  }

  {
    // Pretend to select something that isn't there!
    NSUInteger path[3] = {0, 0, 2};
    NSIndexPath* indexPath = [NSIndexPath indexPathWithIndexes:path length:3];
    [[controller treeController] setSelectionIndexPath:indexPath];
    [controller outlineViewSelectionDidChange:nil];
    EXPECT_FALSE([controller removeButtonEnabled]);
  }

  {
    // Try selecting something that doesn't exist again.
    NSUInteger path[3] = {7, 1, 4};
    NSIndexPath* indexPath = [NSIndexPath indexPathWithIndexes:path length:3];
    [[controller treeController] setSelectionIndexPath:indexPath];
    [controller outlineViewSelectionDidChange:nil];
    EXPECT_FALSE([controller removeButtonEnabled]);
  }

  [controller closeSheet:nil];
}

TEST_F(CookiesWindowControllerTest, UpdateFilter) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(GURL("http://a.com"), "A=B");
  cm->SetCookie(GURL("http://aa.com"), "C=D");
  cm->SetCookie(GURL("http://b.com"), "E=F");
  cm->SetCookie(GURL("http://d.com"), "G=H");
  cm->SetCookie(GURL("http://dd.com"), "I=J");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);

  // Make sure we registered all five cookies.
  EXPECT_EQ(5U, [[[controller_ cocoaTreeModel] children] count]);

  NSSearchField* field =
      [[NSSearchField alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];

  // Make sure we still have five cookies.
  [field setStringValue:@""];
  [controller_ updateFilter:field];
  EXPECT_EQ(5U, [[[controller_ cocoaTreeModel] children] count]);

  // Search for "a".
  [field setStringValue:@"a"];
  [controller_ updateFilter:field];
  EXPECT_EQ(2U, [[[controller_ cocoaTreeModel] children] count]);

  // Search for "b".
  [field setStringValue:@"b"];
  [controller_ updateFilter:field];
  EXPECT_EQ(1U, [[[controller_ cocoaTreeModel] children] count]);

  // Search for "d".
  [field setStringValue:@"d"];
  [controller_ updateFilter:field];
  EXPECT_EQ(2U, [[[controller_ cocoaTreeModel] children] count]);

  // Search for "e".
  [field setStringValue:@"e"];
  [controller_ updateFilter:field];
  EXPECT_EQ(0U, [[[controller_ cocoaTreeModel] children] count]);

  // Search for "aa".
  [field setStringValue:@"aa"];
  [controller_ updateFilter:field];
  EXPECT_EQ(1U, [[[controller_ cocoaTreeModel] children] count]);
}

TEST_F(CookiesWindowControllerTest, CreateDatabaseStorageNodes) {
  TestingProfile* profile = browser_helper_.profile();
  database_helper_ = new MockBrowsingDataDatabaseHelper(profile);
  local_storage_helper_ = new MockBrowsingDataLocalStorageHelper(profile);
  database_helper_->AddDatabaseSamples();
  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);
  database_helper_->Notify();

  ASSERT_EQ(2U, [[[controller_ cocoaTreeModel] children] count]);

  // Root --> gdbhost1.
  CocoaCookieTreeNode* node =
      [[[controller_ cocoaTreeModel] children] objectAtIndex:0];
  EXPECT_NSEQ(@"gdbhost1", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // host1 --> Web Databases.
  node = [[node children] lastObject];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_WEB_DATABASES), [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // Database Storage --> db1.
  node = [[node children] lastObject];
  EXPECT_NSEQ(@"db1", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeTreeDatabase, [node nodeType]);
  CocoaCookieDetails* details = [node details];
  EXPECT_NSEQ(@"description 1", [details databaseDescription]);
  EXPECT_TRUE([details lastModified]);
  EXPECT_TRUE([details fileSize]);

  // Root --> gdbhost2.
  node =
      [[[controller_ cocoaTreeModel] children] objectAtIndex:1];
  EXPECT_NSEQ(@"gdbhost2", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // host1 --> Web Databases.
  node = [[node children] lastObject];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_WEB_DATABASES), [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // Database Storage --> db2.
  node = [[node children] lastObject];
  EXPECT_NSEQ(@"db2", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeTreeDatabase, [node nodeType]);
  details = [node details];
  EXPECT_NSEQ(@"description 2", [details databaseDescription]);
  EXPECT_TRUE([details lastModified]);
  EXPECT_TRUE([details fileSize]);
}

TEST_F(CookiesWindowControllerTest, CreateLocalStorageNodes) {
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(GURL("http://google.com"), "A=B");
  cm->SetCookie(GURL("http://dev.chromium.org"), "C=D");
  database_helper_ = new MockBrowsingDataDatabaseHelper(profile);
  local_storage_helper_ = new MockBrowsingDataLocalStorageHelper(profile);
  local_storage_helper_->AddLocalStorageSamples();
  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile
                                        databaseHelper:database_helper_
                                         storageHelper:local_storage_helper_
                                        appcacheHelper:appcache_helper_
                                       indexedDBHelper:indexed_db_helper_]);
  local_storage_helper_->Notify();

  ASSERT_EQ(4U, [[[controller_ cocoaTreeModel] children] count]);

  // Root --> host1.
  CocoaCookieTreeNode* node =
      [[[controller_ cocoaTreeModel] children] objectAtIndex:2];
  EXPECT_NSEQ(@"host1", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // host1 --> Local Storage.
  node = [[node children] lastObject];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_LOCAL_STORAGE), [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // Local Storage --> http://host1:1/.
  node = [[node children] lastObject];
  EXPECT_NSEQ(@"http://host1:1/", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeTreeLocalStorage, [node nodeType]);
  EXPECT_NSEQ(@"http://host1:1/", [[node details] domain]);
  EXPECT_TRUE([[node details] lastModified]);
  EXPECT_TRUE([[node details] fileSize]);

  // Root --> host2.
  node =
      [[[controller_ cocoaTreeModel] children] objectAtIndex:3];
  EXPECT_NSEQ(@"host2", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // host2 --> Local Storage.
  node = [[node children] lastObject];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_COOKIES_LOCAL_STORAGE), [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeFolder, [node nodeType]);
  EXPECT_EQ(1U, [[node children] count]);

  // Local Storage --> http://host2:2/.
  node = [[node children] lastObject];
  EXPECT_NSEQ(@"http://host2:2/", [node title]);
  EXPECT_EQ(kCocoaCookieDetailsTypeTreeLocalStorage, [node nodeType]);
  EXPECT_NSEQ(@"http://host2:2/", [[node details] domain]);
  EXPECT_TRUE([[node details] lastModified]);
  EXPECT_TRUE([[node details] fileSize]);
}

}  // namespace
