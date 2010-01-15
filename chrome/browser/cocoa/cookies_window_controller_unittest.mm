// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/tree_model.h"
#import "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cookies_window_controller.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Used to test FindCocoaNode. This only sets the title and node, without
// initializing any other members.
@interface FakeCocoaCookieTreeNode : CocoaCookieTreeNode {
  TreeModelNode* testNode_;
}
- (id)initWithTreeNode:(TreeModelNode*)node;
@end
@implementation FakeCocoaCookieTreeNode
- (id)initWithTreeNode:(TreeModelNode*)node {
  if ((self = [super init])) {
    testNode_ = node;
    children_.reset([[NSMutableArray alloc] init]);
  }
  return self;
}
- (TreeModelNode*)treeNode {
  return testNode_;
}
@end

// For some utterly inexplicable reason, |-children| returns an NSArray rather
// than an NSMutableArray, BUT ONLY IN THIS UNIT TEST, and not in the actual
// window controller. This leads to errors for |-addObject:| messages. Maybe
// it's a compiler bug? Or there's something strange happening via swizzling in
// the test environment? Or did somebody forget to sacrifice a lamb on the new
// moon? After investigating for long time, the easiest/best solution was to
// create this hacky secondary getter.
@interface CocoaCookieTreeNode (UglyHacks)
- (NSMutableArray*)childs;
@end
@implementation CocoaCookieTreeNode (UglyHacks)
- (NSMutableArray*)childs {
  return children_.get();
}
@end

namespace {

class CookiesWindowControllerTest : public CocoaTest {
 public:
  CookiesWindowControllerTest()
      : io_thread_(ChromeThread::IO, &message_loop_) {}

  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    profile->CreateRequestContext();
    controller_.reset(
        [[CookiesWindowController alloc] initWithProfile:profile]);
  }

  virtual void TearDown() {
    CocoaTest::TearDown();
  }

  CocoaCookieTreeNode* CocoaNodeFromTreeNode(TreeModelNode* node,
                                             bool recurse) {
    return [controller_ modelObserver]->CocoaNodeFromTreeNode(node, recurse);
  }

  CocoaCookieTreeNode* FindCocoaNode(TreeModelNode* node,
                                     CocoaCookieTreeNode* start) {
    return [controller_ modelObserver]->FindCocoaNode(node, start);
  }

 protected:
  // Prevent leaks from TestingProfile::CreateRequestContext().
  MessageLoop message_loop_;
  ChromeThread io_thread_;

  BrowserTestHelper browser_helper_;

  scoped_nsobject<CookiesWindowController> controller_;
};

TEST_F(CookiesWindowControllerTest, Construction) {
  std::vector<SkBitmap> skia_icons;
  [controller_ treeModel]->GetIcons(&skia_icons);

  EXPECT_EQ([[controller_ icons] count], skia_icons.size() + 1U);
}

TEST_F(CookiesWindowControllerTest, FindCocoaNodeRoot) {
  scoped_ptr< TreeNodeWithValue<int> > search(new TreeNodeWithValue<int>(42));
  scoped_nsobject<FakeCocoaCookieTreeNode> node(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:search.get()]);
  EXPECT_EQ(node.get(), FindCocoaNode(search.get(), node.get()));
}

TEST_F(CookiesWindowControllerTest, FindCocoaNodeImmediateChild) {
  scoped_ptr< TreeNodeWithValue<int> > parent(new TreeNodeWithValue<int>(100));
  scoped_ptr< TreeNodeWithValue<int> > child1(new TreeNodeWithValue<int>(10));
  scoped_ptr< TreeNodeWithValue<int> > child2(new TreeNodeWithValue<int>(20));
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaParent(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:parent.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild1(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child1.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild2(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child2.get()]);
  [[cocoaParent childs] addObject:cocoaChild1.get()];
  [[cocoaParent childs] addObject:cocoaChild2.get()];

  EXPECT_EQ(cocoaChild2.get(), FindCocoaNode(child2.get(), cocoaParent.get()));
}

TEST_F(CookiesWindowControllerTest, FindCocoaNodeRecursive) {
  scoped_ptr< TreeNodeWithValue<int> > parent(new TreeNodeWithValue<int>(100));
  scoped_ptr< TreeNodeWithValue<int> > child1(new TreeNodeWithValue<int>(10));
  scoped_ptr< TreeNodeWithValue<int> > child2(new TreeNodeWithValue<int>(20));
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaParent(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:parent.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild1(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child1.get()]);
  scoped_nsobject<FakeCocoaCookieTreeNode> cocoaChild2(
      [[FakeCocoaCookieTreeNode alloc] initWithTreeNode:child2.get()]);
  [[cocoaParent childs] addObject:cocoaChild1.get()];
  [[cocoaChild1 childs] addObject:cocoaChild2.get()];

  EXPECT_EQ(cocoaChild2.get(), FindCocoaNode(child2.get(), cocoaParent.get()));
}

TEST_F(CookiesWindowControllerTest, CocoaNodeFromTreeNodeCookie) {
  net::CookieMonster* cm = browser_helper_.profile()->GetCookieMonster();
  cm->SetCookie(GURL("http://foo.com"), "A=B");
  CookiesTreeModel model(browser_helper_.profile());

  // Root --> foo.com --> Cookies --> A. Create node for 'A'.
  TreeModelNode* node = model.GetRoot()->GetChild(0)->GetChild(0)->GetChild(0);
  CocoaCookieTreeNode* cookie = CocoaNodeFromTreeNode(node, false);

  EXPECT_TRUE([@"B" isEqualToString:[cookie content]]);
  EXPECT_TRUE([@"End of the session" isEqualToString:[cookie expires]]);
  EXPECT_TRUE([@"Any kind of connection" isEqualToString:[cookie sendFor]]);
  EXPECT_TRUE([@"A" isEqualToString:[cookie title]]);
  EXPECT_TRUE([@"A" isEqualToString:[cookie name]]);
  EXPECT_TRUE([@"/" isEqualToString:[cookie path]]);
  EXPECT_EQ(0U, [[cookie childs] count]);
  EXPECT_TRUE([cookie created]);
  EXPECT_TRUE([cookie isLeaf]);
  EXPECT_EQ(node, [cookie treeNode]);
}

TEST_F(CookiesWindowControllerTest, CocoaNodeFromTreeNodeRecursive) {
  net::CookieMonster* cm = browser_helper_.profile()->GetCookieMonster();
  cm->SetCookie(GURL("http://foo.com"), "A=B");
  CookiesTreeModel model(browser_helper_.profile());

  // Root --> foo.com --> Cookies --> A. Create node for 'foo.com'.
  CookieTreeNode* node = model.GetRoot()->GetChild(0);
  CocoaCookieTreeNode* domain = CocoaNodeFromTreeNode(node, true);
  CocoaCookieTreeNode* cookies = [[domain childs] objectAtIndex:0];
  CocoaCookieTreeNode* cookie = [[cookies childs] objectAtIndex:0];

  // Test domain-level node.
  EXPECT_TRUE([@"foo.com" isEqualToString:[domain title]]);

  EXPECT_FALSE([domain isLeaf]);
  EXPECT_EQ(1U, [[domain childs] count]);
  EXPECT_EQ(node, [domain treeNode]);

  // Test "Cookies" folder node.
  EXPECT_TRUE([@"Cookies" isEqualToString:[cookies title]]);
  EXPECT_FALSE([cookies isLeaf]);
  EXPECT_EQ(1U, [[cookies childs] count]);
  EXPECT_EQ(node->GetChild(0), [cookies treeNode]);

  // Test cookie node. This is the same as CocoaNodeFromTreeNodeCookie.
  EXPECT_TRUE([@"B" isEqualToString:[cookie content]]);
  EXPECT_TRUE([@"End of the session" isEqualToString:[cookie expires]]);
  EXPECT_TRUE([@"Any kind of connection" isEqualToString:[cookie sendFor]]);
  EXPECT_TRUE([@"A" isEqualToString:[cookie title]]);
  EXPECT_TRUE([@"A" isEqualToString:[cookie name]]);
  EXPECT_TRUE([@"/" isEqualToString:[cookie path]]);
  EXPECT_TRUE([@"foo.com" isEqualToString:[cookie domain]]);
  EXPECT_EQ(0U, [[cookie childs] count]);
  EXPECT_TRUE([cookie created]);
  EXPECT_TRUE([cookie isLeaf]);
  EXPECT_EQ(node->GetChild(0)->GetChild(0), [cookie treeNode]);
}

TEST_F(CookiesWindowControllerTest, TreeNodesAdded) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile]);

  // Root --> foo.com --> Cookies.
  NSMutableArray* cocoa_children =
      [[[[[[controller_ cocoaTreeModel] childs] objectAtIndex:0]
          childs] objectAtIndex:0] childs];
  EXPECT_EQ(1U, [cocoa_children count]);

  // Create some cookies.
  cm->SetCookie(url, "C=D");
  cm->SetCookie(url, "E=F");

  net::CookieMonster::CookieList list = cm->GetAllCookies();
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
      [[CookiesWindowController alloc] initWithProfile:profile]);

  // Root --> foo.com --> Cookies.
  NSMutableArray* cocoa_children =
      [[[[[[controller_ cocoaTreeModel] childs] objectAtIndex:0]
          childs] objectAtIndex:0] childs];
  EXPECT_EQ(3U, [cocoa_children count]);

  CookiesTreeModel* model = [controller_ treeModel];
  // Root --> foo.com --> Cookies.
  CookieTreeNode* parent = model->GetRoot()->GetChild(0)->GetChild(0);

  // Pretend to remove the nodes.
  [controller_ modelObserver]->TreeNodesRemoved(model, parent, 1, 2);

  EXPECT_EQ(1U, [cocoa_children count]);

  NSString* title = [[cocoa_children objectAtIndex:0] name];
  EXPECT_TRUE([@"A" isEqualToString:title]);
}

TEST_F(CookiesWindowControllerTest, TreeNodeChildrenReordered) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");
  cm->SetCookie(url, "C=D");
  cm->SetCookie(url, "E=F");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile]);

  // Root --> foo.com --> Cookies.
  NSMutableArray* cocoa_children =
      [[[[[[controller_ cocoaTreeModel] childs] objectAtIndex:0]
          childs] objectAtIndex:0] childs];
  EXPECT_EQ(3U, [cocoa_children count]);

  // Check default ordering.
  CocoaCookieTreeNode* node = [cocoa_children objectAtIndex:0];
  EXPECT_TRUE([@"A" isEqualToString:[node name]]);
  node =  [cocoa_children objectAtIndex:1];
  EXPECT_TRUE([@"C" isEqualToString:[node name]]);
  node =  [cocoa_children objectAtIndex:2];
  EXPECT_TRUE([@"E" isEqualToString:[node name]]);

  CookiesTreeModel* model = [controller_ treeModel];
  // Root --> foo.com --> Cookies.
  CookieTreeNode* parent = model->GetRoot()->GetChild(0)->GetChild(0);

  // Reorder the nodes.
  CookieTreeNode* node_e = parent->Remove(2);
  CookieTreeNode* node_c = parent->Remove(1);
  parent->Add(0, node_e);
  parent->Add(2, node_c);

  // Notify the observer of the reordering.
  [controller_ modelObserver]->TreeNodeChildrenReordered(model, parent);

  // Check the new order.
  node = [cocoa_children objectAtIndex:0];
  EXPECT_TRUE([@"E" isEqualToString:[node name]]);
  node =  [cocoa_children objectAtIndex:1];
  EXPECT_TRUE([@"A" isEqualToString:[node name]]);
  node =  [cocoa_children objectAtIndex:2];
  EXPECT_TRUE([@"C" isEqualToString:[node name]]);
}

TEST_F(CookiesWindowControllerTest, TreeNodeChanged) {
  const GURL url = GURL("http://foo.com");
  TestingProfile* profile = browser_helper_.profile();
  net::CookieMonster* cm = profile->GetCookieMonster();
  cm->SetCookie(url, "A=B");

  controller_.reset(
      [[CookiesWindowController alloc] initWithProfile:profile]);

  CookiesTreeModel* model = [controller_ treeModel];
  // Root --> foo.com --> Cookies.
  CookieTreeNode* node = model->GetRoot()->GetChild(0)->GetChild(0);

  // Root --> foo.com --> Cookies.
  CocoaCookieTreeNode* cocoa_node =
      [[[[[controller_ cocoaTreeModel] childs] objectAtIndex:0]
          childs] objectAtIndex:0];

  EXPECT_TRUE([@"Cookies" isEqualToString:[cocoa_node title]]);

  // Fake update the cookie folder's title. This would never happen in reality,
  // but it tests the code path that ultimately calls CocoaNodeFromTreeNode,
  // which is tested elsewhere.
  node->SetTitle(L"Silly Change");
  [controller_ modelObserver]->TreeNodeChanged(model, node);

  EXPECT_TRUE([@"Silly Change" isEqualToString:[cocoa_node title]]);
}

}  // namespace
