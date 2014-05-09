// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace services {
namespace view_manager {

base::RunLoop* current_run_loop = NULL;

void DoRunLoop() {
  base::RunLoop run_loop;
  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = NULL;
}

void QuitRunLoop() {
  current_run_loop->Quit();
}

// ViewManager -----------------------------------------------------------------

// These tests model synchronization of two peer connections to the view manager
// service, that are given access to some root node.

class ViewManagerTest : public testing::Test {
 public:
  ViewManagerTest() : commit_count_(0) {}

 protected:
  ViewManager* view_manager_1() { return view_manager_1_.get(); }
  ViewManager* view_manager_2() { return view_manager_2_.get(); }

  ViewTreeNode* CreateNodeInParent(ViewTreeNode* parent) {
    ViewManager* parent_manager = ViewTreeNodePrivate(parent).view_manager();
    ViewTreeNode* node = ViewTreeNode::Create(parent_manager);
    parent->AddChild(node);
    return node;
  }

  void DestroyViewManager1() {
    view_manager_1_.reset();
  }

 private:
  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    test_helper_.Init();
    view_manager_1_.reset(new ViewManager(test_helper_.shell()));
    view_manager_2_.reset(new ViewManager(test_helper_.shell()));
    view_manager_1_->Init();
    view_manager_2_->Init();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;
  scoped_ptr<ViewManager> view_manager_1_;
  scoped_ptr<ViewManager> view_manager_2_;
  int commit_count_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

// Base class for helpers that quit the current runloop after a specific tree
// change is observed by a view manager.
class TreeObserverBase : public ViewTreeNodeObserver {
 public:
  explicit TreeObserverBase(ViewManager* view_manager)
      : view_manager_(view_manager) {
    view_manager_->tree()->AddObserver(this);
  }
  virtual ~TreeObserverBase() {
    view_manager_->tree()->RemoveObserver(this);
  }

 protected:
  virtual bool ShouldQuitRunLoop(const TreeChangeParams& params) = 0;

  ViewManager* view_manager() { return view_manager_; }

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnTreeChange(const TreeChangeParams& params) OVERRIDE {
    if (ShouldQuitRunLoop(params))
      QuitRunLoop();
  }

  ViewManager* view_manager_;
  DISALLOW_COPY_AND_ASSIGN(TreeObserverBase);
};

// Spins a runloop until the tree beginning at |root| has |tree_size| nodes
// (including |root|).
class TreeSizeMatchesWaiter : public TreeObserverBase {
 public:
  TreeSizeMatchesWaiter(ViewManager* view_manager, size_t tree_size)
      : TreeObserverBase(view_manager),
        tree_size_(tree_size) {
    DoRunLoop();
  }
  virtual ~TreeSizeMatchesWaiter() {}

 private:
  // Overridden from TreeObserverBase:
  virtual bool ShouldQuitRunLoop(const TreeChangeParams& params) OVERRIDE {
    if (params.phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return false;
    return CountNodes(view_manager()->tree()) == tree_size_;
  }

  size_t CountNodes(ViewTreeNode* node) const {
    size_t count = 1;
    ViewTreeNode::Children::const_iterator it = node->children().begin();
    for (; it != node->children().end(); ++it)
      count += CountNodes(*it);
    return count;
  }

  size_t tree_size_;
  DISALLOW_COPY_AND_ASSIGN(TreeSizeMatchesWaiter);
};


class HierarchyChanged_NodeCreatedObserver : public TreeObserverBase {
 public:
  explicit HierarchyChanged_NodeCreatedObserver(ViewManager* view_manager)
      : TreeObserverBase(view_manager) {}
  virtual ~HierarchyChanged_NodeCreatedObserver() {}

 private:
  // Overridden from TreeObserverBase:
  virtual bool ShouldQuitRunLoop(const TreeChangeParams& params) OVERRIDE {
    if (params.phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return false;
    return params.receiver == view_manager()->tree() &&
        !params.old_parent &&
        params.new_parent == view_manager()->tree();
  }

  DISALLOW_COPY_AND_ASSIGN(HierarchyChanged_NodeCreatedObserver);
};

TEST_F(ViewManagerTest, HierarchyChanged_NodeCreated) {
  HierarchyChanged_NodeCreatedObserver observer(view_manager_2());
  ViewTreeNode* node1 = ViewTreeNode::Create(view_manager_1());
  view_manager_1()->tree()->AddChild(node1);
  DoRunLoop();

  EXPECT_EQ(view_manager_2()->tree()->children().front()->id(), node1->id());
}

// Quits the current runloop when the root is notified about a node moved from
// |old_parent_id| to |new_parent_id|.
class HierarchyChanged_NodeMovedObserver : public TreeObserverBase {
 public:
  HierarchyChanged_NodeMovedObserver(ViewManager* view_manager,
                                     TransportNodeId old_parent_id,
                                     TransportNodeId new_parent_id)
      : TreeObserverBase(view_manager),
        old_parent_id_(old_parent_id),
        new_parent_id_(new_parent_id) {}
  virtual ~HierarchyChanged_NodeMovedObserver() {}

 private:
  // Overridden from TreeObserverBase:
  virtual bool ShouldQuitRunLoop(const TreeChangeParams& params) OVERRIDE {
    if (params.phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return false;
    return params.receiver == view_manager()->tree() &&
        params.old_parent->id() == old_parent_id_&&
        params.new_parent->id() == new_parent_id_;
  }

  TransportNodeId old_parent_id_;
  TransportNodeId new_parent_id_;

  DISALLOW_COPY_AND_ASSIGN(HierarchyChanged_NodeMovedObserver);
};

TEST_F(ViewManagerTest, HierarchyChanged_NodeMoved) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  ViewTreeNode* node2 = CreateNodeInParent(view_manager_1()->tree());
  ViewTreeNode* node21 = CreateNodeInParent(node2);
  TreeSizeMatchesWaiter waiter(view_manager_2(), 4);

  HierarchyChanged_NodeMovedObserver observer(view_manager_2(),
                                              node2->id(),
                                              node1->id());

  node1->AddChild(node21);
  DoRunLoop();

  ViewTreeNode* tree2 = view_manager_2()->tree();

  EXPECT_EQ(tree2->children().size(), 2u);
  ViewTreeNode* tree2_node1 = tree2->GetChildById(node1->id());
  EXPECT_EQ(tree2_node1->children().size(), 1u);
  ViewTreeNode* tree2_node2 = tree2->GetChildById(node2->id());
  EXPECT_TRUE(tree2_node2->children().empty());
  ViewTreeNode* tree2_node21 = tree2->GetChildById(node21->id());
  EXPECT_EQ(tree2_node21->parent(), tree2_node1);
}

class HierarchyChanged_NodeRemovedObserver : public TreeObserverBase {
 public:
  HierarchyChanged_NodeRemovedObserver(ViewManager* view_manager)
      : TreeObserverBase(view_manager) {}
  virtual ~HierarchyChanged_NodeRemovedObserver() {}

 private:
  // Overridden from TreeObserverBase:
  virtual bool ShouldQuitRunLoop(const TreeChangeParams& params) OVERRIDE {
    if (params.phase != ViewTreeNodeObserver::DISPOSITION_CHANGING)
      return false;
    return params.receiver == view_manager()->tree() &&
        params.old_parent->id() == params.receiver->id() &&
        params.new_parent == 0;
  }

  DISALLOW_COPY_AND_ASSIGN(HierarchyChanged_NodeRemovedObserver);
};

TEST_F(ViewManagerTest, HierarchyChanged_NodeRemoved) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  TreeSizeMatchesWaiter waiter(view_manager_2(), 2);

  HierarchyChanged_NodeRemovedObserver observer(view_manager_2());

  view_manager_1()->tree()->RemoveChild(node1);
  DoRunLoop();

  ViewTreeNode* tree2 = view_manager_2()->tree();

  EXPECT_TRUE(tree2->children().empty());
}

class NodeDestroyed_Waiter : public ViewTreeNodeObserver {
 public:
  NodeDestroyed_Waiter(ViewManager* view_manager, TransportNodeId id)
      : view_manager_(view_manager),
        id_(id) {
    view_manager_->GetNodeById(id)->AddObserver(this);
    DoRunLoop();
  }
  virtual ~NodeDestroyed_Waiter() {
  }

 private:
  // Overridden from TreeObserverBase:
  virtual void OnNodeDestroy(ViewTreeNode* node,
                             DispositionChangePhase phase) OVERRIDE {
    if (phase != DISPOSITION_CHANGED)
      return;
    if (node->id() == id_)
      QuitRunLoop();
  }

  TransportNodeId id_;
  ViewManager* view_manager_;

  DISALLOW_COPY_AND_ASSIGN(NodeDestroyed_Waiter);
};

TEST_F(ViewManagerTest, NodeDestroyed) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  TreeSizeMatchesWaiter init_waiter(view_manager_2(), 2);

  // |node1| will be deleted after calling Destroy() below.
  TransportNodeId id = node1->id();
  node1->Destroy();
  NodeDestroyed_Waiter destroyed_waiter(view_manager_2(), id);

  EXPECT_TRUE(view_manager_2()->tree()->children().empty());
}

TEST_F(ViewManagerTest, ViewManagerDestroyed) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  TreeSizeMatchesWaiter init_waiter(view_manager_2(), 2);

  TransportNodeId id = node1->id();
  DestroyViewManager1();
  NodeDestroyed_Waiter destroyed_waiter(view_manager_2(), id);

  // tree() should still be valid, since it's owned by neither connection.
  EXPECT_TRUE(view_manager_2()->tree()->children().empty());
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
