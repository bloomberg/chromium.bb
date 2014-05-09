// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

#include "base/logging.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace services {
namespace view_manager {

// ViewTreeNode ----------------------------------------------------------------

typedef testing::Test ViewTreeNodeTest;

TEST_F(ViewTreeNodeTest, AddChild) {
  ViewTreeNode v1;
  ViewTreeNode* v11 = new ViewTreeNode;
  v1.AddChild(v11);
  EXPECT_EQ(1U, v1.children().size());
}

TEST_F(ViewTreeNodeTest, RemoveChild) {
  ViewTreeNode v1;
  ViewTreeNode* v11 = new ViewTreeNode;
  v1.AddChild(v11);
  EXPECT_EQ(1U, v1.children().size());
  v1.RemoveChild(v11);
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(ViewTreeNodeTest, Reparent) {
  ViewTreeNode v1;
  ViewTreeNode v2;
  ViewTreeNode* v11 = new ViewTreeNode;
  v1.AddChild(v11);
  EXPECT_EQ(1U, v1.children().size());
  v2.AddChild(v11);
  EXPECT_EQ(1U, v2.children().size());
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(ViewTreeNodeTest, Contains) {
  ViewTreeNode v1;

  // Direct descendant.
  ViewTreeNode* v11 = new ViewTreeNode;
  v1.AddChild(v11);
  EXPECT_TRUE(v1.Contains(v11));

  // Indirect descendant.
  ViewTreeNode* v111 = new ViewTreeNode;
  v11->AddChild(v111);
  EXPECT_TRUE(v1.Contains(v111));
}

TEST_F(ViewTreeNodeTest, GetChildById) {
  ViewTreeNode v1;
  ViewTreeNodePrivate(&v1).set_id(1);
  ViewTreeNode v11;
  ViewTreeNodePrivate(&v11).set_id(11);
  v1.AddChild(&v11);
  ViewTreeNode v111;
  ViewTreeNodePrivate(&v111).set_id(111);
  v11.AddChild(&v111);

  // Find direct & indirect descendents.
  EXPECT_EQ(&v11, v1.GetChildById(v11.id()));
  EXPECT_EQ(&v111, v1.GetChildById(v111.id()));
}

// ViewTreeNodeObserver --------------------------------------------------------

typedef testing::Test ViewTreeNodeObserverTest;

bool TreeChangeParamsMatch(const ViewTreeNodeObserver::TreeChangeParams& lhs,
                           const ViewTreeNodeObserver::TreeChangeParams& rhs) {
  return lhs.target == rhs.target &&  lhs.old_parent == rhs.old_parent &&
      lhs.new_parent == rhs.new_parent && lhs.receiver == rhs.receiver &&
      lhs.phase == rhs.phase;
}

class TreeChangeObserver : public ViewTreeNodeObserver {
 public:
  explicit TreeChangeObserver(ViewTreeNode* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  virtual ~TreeChangeObserver() {
    observee_->RemoveObserver(this);
  }

  void Reset() {
    received_params_.clear();
  }

  const std::vector<TreeChangeParams>& received_params() {
    return received_params_;
  }

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnTreeChange(const TreeChangeParams& params) OVERRIDE {
    received_params_.push_back(params);
  }

  ViewTreeNode* observee_;
  std::vector<TreeChangeParams> received_params_;

  DISALLOW_COPY_AND_ASSIGN(TreeChangeObserver);
};

// Adds/Removes v11 to v1.
TEST_F(ViewTreeNodeObserverTest, TreeChange_SimpleAddRemove) {
  ViewTreeNode v1;
  TreeChangeObserver o1(&v1);
  EXPECT_TRUE(o1.received_params().empty());

  ViewTreeNode v11;
  v11.set_owned_by_parent(false);
  TreeChangeObserver o11(&v11);
  EXPECT_TRUE(o11.received_params().empty());

  // Add.

  v1.AddChild(&v11);

  EXPECT_EQ(1U, o1.received_params().size());
  ViewTreeNodeObserver::TreeChangeParams p1;
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v1;
  p1.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  ViewTreeNodeObserver::TreeChangeParams p11 = p1;
  p11.receiver = &v11;
  p11.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  p11.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  o1.Reset();
  o11.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());

  // Remove.

  v1.RemoveChild(&v11);

  EXPECT_EQ(1U, o1.received_params().size());
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = &v1;
  p1.new_parent = NULL;
  p1.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  p11.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));
}

// Creates these two trees:
// v1
//  +- v11
// v111
//  +- v1111
//  +- v1112
// Then adds/removes v111 from v11.
TEST_F(ViewTreeNodeObserverTest, TreeChange_NestedAddRemove) {
  ViewTreeNode v1, v11, v111, v1111, v1112;

  // Root tree.
  v11.set_owned_by_parent(false);
  v1.AddChild(&v11);

  // Tree to be attached.
  v111.set_owned_by_parent(false);
  v1111.set_owned_by_parent(false);
  v111.AddChild(&v1111);
  v1112.set_owned_by_parent(false);
  v111.AddChild(&v1112);

  TreeChangeObserver o1(&v1), o11(&v11), o111(&v111), o1111(&v1111),
      o1112(&v1112);
  ViewTreeNodeObserver::TreeChangeParams p1, p11, p111, p1111, p1112;

  // Add.

  v11.AddChild(&v111);

  EXPECT_EQ(1U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v11;
  p1.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(1U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  p111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  p111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  p1111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  p1111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
  p1112.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  p1112.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));

  // Remove.
  o1.Reset();
  o11.Reset();
  o111.Reset();
  o1111.Reset();
  o1112.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());
  EXPECT_TRUE(o111.received_params().empty());
  EXPECT_TRUE(o1111.received_params().empty());
  EXPECT_TRUE(o1112.received_params().empty());

  v11.RemoveChild(&v111);

  EXPECT_EQ(1U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = &v11;
  p1.new_parent = NULL;
  p1.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(1U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  p111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  p111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  p1111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  p1111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
  p1112.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  p1112.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));
}

TEST_F(ViewTreeNodeObserverTest, TreeChange_Reparent) {
  ViewTreeNode v1, v11, v12, v111;
  v11.set_owned_by_parent(false);
  v111.set_owned_by_parent(false);
  v12.set_owned_by_parent(false);
  v1.AddChild(&v11);
  v1.AddChild(&v12);
  v11.AddChild(&v111);

  TreeChangeObserver o1(&v1), o11(&v11), o12(&v12), o111(&v111);

  // Reparent.
  v12.AddChild(&v111);

  // v1 (root) should see both changing and changed notifications.
  EXPECT_EQ(2U, o1.received_params().size());
  ViewTreeNodeObserver::TreeChangeParams p1;
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = &v11;
  p1.new_parent = &v12;
  p1.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));
  p1.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  // v11 should see changing notifications.
  EXPECT_EQ(1U, o11.received_params().size());
  ViewTreeNodeObserver::TreeChangeParams p11;
  p11 = p1;
  p11.receiver = &v11;
  p11.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  // v12 should see changed notifications.
  EXPECT_EQ(1U, o12.received_params().size());
  ViewTreeNodeObserver::TreeChangeParams p12;
  p12 = p1;
  p12.receiver = &v12;
  p12.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p12, o12.received_params().back()));

  // v111 should see both changing and changed notifications.
  EXPECT_EQ(2U, o111.received_params().size());
  ViewTreeNodeObserver::TreeChangeParams p111;
  p111 = p1;
  p111.receiver = &v111;
  p111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGING;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  p111.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
