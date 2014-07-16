// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/node.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "mojo/services/public/cpp/view_manager/lib/node_private.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace view_manager {

// Node ------------------------------------------------------------------------

typedef testing::Test NodeTest;

// Subclass with public ctor/dtor.
class TestNode : public Node {
 public:
  TestNode() {
    NodePrivate(this).set_id(1);
  }
  ~TestNode() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNode);
};

TEST_F(NodeTest, AddChild) {
  TestNode v1;
  TestNode v11;
  v1.AddChild(&v11);
  EXPECT_EQ(1U, v1.children().size());
}

TEST_F(NodeTest, RemoveChild) {
  TestNode v1;
  TestNode v11;
  v1.AddChild(&v11);
  EXPECT_EQ(1U, v1.children().size());
  v1.RemoveChild(&v11);
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(NodeTest, Reparent) {
  TestNode v1;
  TestNode v2;
  TestNode v11;
  v1.AddChild(&v11);
  EXPECT_EQ(1U, v1.children().size());
  v2.AddChild(&v11);
  EXPECT_EQ(1U, v2.children().size());
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(NodeTest, Contains) {
  TestNode v1;

  // Direct descendant.
  TestNode v11;
  v1.AddChild(&v11);
  EXPECT_TRUE(v1.Contains(&v11));

  // Indirect descendant.
  TestNode v111;
  v11.AddChild(&v111);
  EXPECT_TRUE(v1.Contains(&v111));
}

TEST_F(NodeTest, GetChildById) {
  TestNode v1;
  NodePrivate(&v1).set_id(1);
  TestNode v11;
  NodePrivate(&v11).set_id(11);
  v1.AddChild(&v11);
  TestNode v111;
  NodePrivate(&v111).set_id(111);
  v11.AddChild(&v111);

  // Find direct & indirect descendents.
  EXPECT_EQ(&v11, v1.GetChildById(v11.id()));
  EXPECT_EQ(&v111, v1.GetChildById(v111.id()));
}

// NodeObserver --------------------------------------------------------

typedef testing::Test NodeObserverTest;

bool TreeChangeParamsMatch(const NodeObserver::TreeChangeParams& lhs,
                           const NodeObserver::TreeChangeParams& rhs) {
  return lhs.target == rhs.target &&  lhs.old_parent == rhs.old_parent &&
      lhs.new_parent == rhs.new_parent && lhs.receiver == rhs.receiver;
}

class TreeChangeObserver : public NodeObserver {
 public:
  explicit TreeChangeObserver(Node* observee) : observee_(observee) {
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
  // Overridden from NodeObserver:
   virtual void OnTreeChanging(const TreeChangeParams& params) OVERRIDE {
     received_params_.push_back(params);
   }
  virtual void OnTreeChanged(const TreeChangeParams& params) OVERRIDE {
    received_params_.push_back(params);
  }

  Node* observee_;
  std::vector<TreeChangeParams> received_params_;

  DISALLOW_COPY_AND_ASSIGN(TreeChangeObserver);
};

// Adds/Removes v11 to v1.
TEST_F(NodeObserverTest, TreeChange_SimpleAddRemove) {
  TestNode v1;
  TreeChangeObserver o1(&v1);
  EXPECT_TRUE(o1.received_params().empty());

  TestNode v11;
  TreeChangeObserver o11(&v11);
  EXPECT_TRUE(o11.received_params().empty());

  // Add.

  v1.AddChild(&v11);

  EXPECT_EQ(2U, o1.received_params().size());
  NodeObserver::TreeChangeParams p1;
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v1;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  NodeObserver::TreeChangeParams p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  o1.Reset();
  o11.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());

  // Remove.

  v1.RemoveChild(&v11);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = &v1;
  p1.new_parent = NULL;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));
}

// Creates these two trees:
// v1
//  +- v11
// v111
//  +- v1111
//  +- v1112
// Then adds/removes v111 from v11.
TEST_F(NodeObserverTest, TreeChange_NestedAddRemove) {
  TestNode v1, v11, v111, v1111, v1112;

  // Root tree.
  v1.AddChild(&v11);

  // Tree to be attached.
  v111.AddChild(&v1111);
  v111.AddChild(&v1112);

  TreeChangeObserver o1(&v1), o11(&v11), o111(&v111), o1111(&v1111),
      o1112(&v1112);
  NodeObserver::TreeChangeParams p1, p11, p111, p1111, p1112;

  // Add.

  v11.AddChild(&v111);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
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

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = &v11;
  p1.new_parent = NULL;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));
}

TEST_F(NodeObserverTest, TreeChange_Reparent) {
  TestNode v1, v11, v12, v111;
  v1.AddChild(&v11);
  v1.AddChild(&v12);
  v11.AddChild(&v111);

  TreeChangeObserver o1(&v1), o11(&v11), o12(&v12), o111(&v111);

  // Reparent.
  v12.AddChild(&v111);

  // v1 (root) should see both changing and changed notifications.
  EXPECT_EQ(4U, o1.received_params().size());
  NodeObserver::TreeChangeParams p1;
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = &v11;
  p1.new_parent = &v12;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  // v11 should see changing notifications.
  EXPECT_EQ(2U, o11.received_params().size());
  NodeObserver::TreeChangeParams p11;
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));

  // v12 should see changed notifications.
  EXPECT_EQ(2U, o12.received_params().size());
  NodeObserver::TreeChangeParams p12;
  p12 = p1;
  p12.receiver = &v12;
  EXPECT_TRUE(TreeChangeParamsMatch(p12, o12.received_params().back()));

  // v111 should see both changing and changed notifications.
  EXPECT_EQ(2U, o111.received_params().size());
  NodeObserver::TreeChangeParams p111;
  p111 = p1;
  p111.receiver = &v111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));
}

namespace {

class OrderChangeObserver : public NodeObserver {
 public:
  struct Change {
    Node* node;
    Node* relative_node;
    OrderDirection direction;
  };
  typedef std::vector<Change> Changes;

  explicit OrderChangeObserver(Node* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  virtual ~OrderChangeObserver() {
    observee_->RemoveObserver(this);
  }

  Changes GetAndClearChanges() {
    Changes changes;
    changes_.swap(changes);
    return changes;
  }

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeReordering(Node* node,
                                Node* relative_node,
                                OrderDirection direction) OVERRIDE {
    OnNodeReordered(node, relative_node, direction);
  }

  virtual void OnNodeReordered(Node* node,
                               Node* relative_node,
                               OrderDirection direction) OVERRIDE {
    Change change;
    change.node = node;
    change.relative_node = relative_node;
    change.direction = direction;
    changes_.push_back(change);
  }

  Node* observee_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

}  // namespace

TEST_F(NodeObserverTest, Order) {
  TestNode v1, v11, v12, v13;
  v1.AddChild(&v11);
  v1.AddChild(&v12);
  v1.AddChild(&v13);

  // Order: v11, v12, v13
  EXPECT_EQ(3U, v1.children().size());
  EXPECT_EQ(&v11, v1.children().front());
  EXPECT_EQ(&v13, v1.children().back());

  {
    OrderChangeObserver observer(&v11);

    // Move v11 to front.
    // Resulting order: v12, v13, v11
    v11.MoveToFront();
    EXPECT_EQ(&v12, v1.children().front());
    EXPECT_EQ(&v11, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    EXPECT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].node);
    EXPECT_EQ(&v13, changes[0].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].node);
    EXPECT_EQ(&v13, changes[1].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&v11);

    // Move v11 to back.
    // Resulting order: v11, v12, v13
    v11.MoveToBack();
    EXPECT_EQ(&v11, v1.children().front());
    EXPECT_EQ(&v13, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    EXPECT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].node);
    EXPECT_EQ(&v12, changes[0].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].node);
    EXPECT_EQ(&v12, changes[1].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&v11);

    // Move v11 above v12.
    // Resulting order: v12. v11, v13
    v11.Reorder(&v12, ORDER_DIRECTION_ABOVE);
    EXPECT_EQ(&v12, v1.children().front());
    EXPECT_EQ(&v13, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    EXPECT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].node);
    EXPECT_EQ(&v12, changes[0].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].node);
    EXPECT_EQ(&v12, changes[1].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&v11);

    // Move v11 below v12.
    // Resulting order: v11, v12, v13
    v11.Reorder(&v12, ORDER_DIRECTION_BELOW);
    EXPECT_EQ(&v11, v1.children().front());
    EXPECT_EQ(&v13, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    EXPECT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].node);
    EXPECT_EQ(&v12, changes[0].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].node);
    EXPECT_EQ(&v12, changes[1].relative_node);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[1].direction);
  }
}

namespace {

typedef std::vector<std::string> Changes;

std::string NodeIdToString(Id id) {
  return (id == 0) ? "null" :
      base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

std::string RectToString(const gfx::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d",
                            rect.x(), rect.y(), rect.width(), rect.height());
}

class BoundsChangeObserver : public NodeObserver {
 public:
  explicit BoundsChangeObserver(Node* node) : node_(node) {
    node_->AddObserver(this);
  }
  virtual ~BoundsChangeObserver() {
    node_->RemoveObserver(this);
  }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from NodeObserver:
  virtual void OnNodeBoundsChanging(Node* node,
                                    const gfx::Rect& old_bounds,
                                    const gfx::Rect& new_bounds) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "node=%s old_bounds=%s new_bounds=%s phase=changing",
            NodeIdToString(node->id()).c_str(),
            RectToString(old_bounds).c_str(),
            RectToString(new_bounds).c_str()));
  }
  virtual void OnNodeBoundsChanged(Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "node=%s old_bounds=%s new_bounds=%s phase=changed",
            NodeIdToString(node->id()).c_str(),
            RectToString(old_bounds).c_str(),
            RectToString(new_bounds).c_str()));
  }

  Node* node_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

}  // namespace

TEST_F(NodeObserverTest, SetBounds) {
  TestNode v1;
  {
    BoundsChangeObserver observer(&v1);
    v1.SetBounds(gfx::Rect(0, 0, 100, 100));

    Changes changes = observer.GetAndClearChanges();
    EXPECT_EQ(2U, changes.size());
    EXPECT_EQ(
        "node=0,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changing",
        changes[0]);
    EXPECT_EQ(
        "node=0,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changed",
        changes[1]);
  }
}

}  // namespace view_manager
}  // namespace mojo
