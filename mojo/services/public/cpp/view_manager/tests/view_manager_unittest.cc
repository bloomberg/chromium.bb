// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace view_manager {
namespace {

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

void QuitRunLoopOnChangesAcked() {
  QuitRunLoop();
}

void WaitForAllChangesToBeAcked(ViewManager* manager) {
  ViewManagerPrivate(manager).synchronizer()->set_changes_acked_callback(
      base::Bind(&QuitRunLoopOnChangesAcked));
  DoRunLoop();
  ViewManagerPrivate(manager).synchronizer()->ClearChangesAckedCallback();
}

class ActiveViewChangedObserver : public ViewTreeNodeObserver {
 public:
  explicit ActiveViewChangedObserver(ViewTreeNode* node)
      : node_(node) {}
  virtual ~ActiveViewChangedObserver() {}

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnNodeActiveViewChange(ViewTreeNode* node,
                                      View* old_view,
                                      View* new_view,
                                      DispositionChangePhase phase) OVERRIDE {
    DCHECK_EQ(node, node_);
    QuitRunLoop();
  }

  ViewTreeNode* node_;

  DISALLOW_COPY_AND_ASSIGN(ActiveViewChangedObserver);
};

// Waits until the active view id of the supplied node changes.
void WaitForActiveViewToChange(ViewTreeNode* node) {
  ActiveViewChangedObserver observer(node);
  node->AddObserver(&observer);
  DoRunLoop();
  node->RemoveObserver(&observer);
}

class BoundsChangeObserver : public ViewTreeNodeObserver {
 public:
  explicit BoundsChangeObserver(ViewTreeNode* node) : node_(node) {}
  virtual ~BoundsChangeObserver() {}

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnNodeBoundsChange(ViewTreeNode* node,
                                  const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds,
                                  DispositionChangePhase phase) OVERRIDE {
    DCHECK_EQ(node, node_);
    if (phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return;
    QuitRunLoop();
  }

  ViewTreeNode* node_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

// Wait until the bounds of the supplied node change.
void WaitForBoundsToChange(ViewTreeNode* node) {
  BoundsChangeObserver observer(node);
  node->AddObserver(&observer);
  DoRunLoop();
  node->RemoveObserver(&observer);
}

// Spins a runloop until the tree beginning at |root| has |tree_size| nodes
// (including |root|).
class TreeSizeMatchesObserver : public ViewTreeNodeObserver {
 public:
  TreeSizeMatchesObserver(ViewTreeNode* tree, size_t tree_size)
      : tree_(tree),
        tree_size_(tree_size) {}
  virtual ~TreeSizeMatchesObserver() {}

  bool IsTreeCorrectSize() {
    return CountNodes(tree_) == tree_size_;
  }

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnTreeChange(const TreeChangeParams& params) OVERRIDE {
    if (params.phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return;
    if (IsTreeCorrectSize())
      QuitRunLoop();
  }

  size_t CountNodes(const ViewTreeNode* node) const {
    size_t count = 1;
    ViewTreeNode::Children::const_iterator it = node->children().begin();
    for (; it != node->children().end(); ++it)
      count += CountNodes(*it);
    return count;
  }

  ViewTreeNode* tree_;
  size_t tree_size_;
  DISALLOW_COPY_AND_ASSIGN(TreeSizeMatchesObserver);
};

void WaitForTreeSizeToMatch(ViewTreeNode* node, size_t tree_size) {
  TreeSizeMatchesObserver observer(node, tree_size);
  if (observer.IsTreeCorrectSize())
    return;
  node->AddObserver(&observer);
  DoRunLoop();
  node->RemoveObserver(&observer);
}


// Utility class that waits for the destruction of some number of nodes and
// views.
class DestructionObserver : public ViewTreeNodeObserver,
                            public ViewObserver {
 public:
  // |nodes| or |views| can be NULL.
  DestructionObserver(std::set<TransportNodeId>* nodes,
                      std::set<TransportViewId>* views)
      : nodes_(nodes),
        views_(views) {}

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnNodeDestroy(
      ViewTreeNode* node,
      ViewTreeNodeObserver::DispositionChangePhase phase) OVERRIDE {
    if (phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return;
    std::set<TransportNodeId>::const_iterator it = nodes_->find(node->id());
    if (it != nodes_->end())
      nodes_->erase(it);
    if (CanQuit())
      QuitRunLoop();
  }

  // Overridden from ViewObserver:
  virtual void OnViewDestroy(
      View* view,
      ViewObserver::DispositionChangePhase phase) OVERRIDE {
    if (phase != ViewObserver::DISPOSITION_CHANGED)
      return;
    std::set<TransportViewId>::const_iterator it = views_->find(view->id());
    if (it != views_->end())
      views_->erase(it);
    if (CanQuit())
      QuitRunLoop();
  }

  bool CanQuit() {
    return (!nodes_ || nodes_->empty()) && (!views_ || views_->empty());
  }

  std::set<TransportNodeId>* nodes_;
  std::set<TransportViewId>* views_;

  DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
};

void WaitForDestruction(ViewManager* view_manager,
                        std::set<TransportNodeId>* nodes,
                        std::set<TransportViewId>* views) {
  DestructionObserver observer(nodes, views);
  DCHECK(nodes || views);
  if (nodes) {
    for (std::set<TransportNodeId>::const_iterator it = nodes->begin();
          it != nodes->end(); ++it) {
      view_manager->GetNodeById(*it)->AddObserver(&observer);
    }
  }
  if (views) {
    for (std::set<TransportViewId>::const_iterator it = views->begin();
          it != views->end(); ++it) {
      view_manager->GetViewById(*it)->AddObserver(&observer);
    }
  }
  DoRunLoop();
}

}  // namespace

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
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 4);

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
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  HierarchyChanged_NodeRemovedObserver observer(view_manager_2());

  view_manager_1()->tree()->RemoveChild(node1);
  DoRunLoop();

  ViewTreeNode* tree2 = view_manager_2()->tree();

  EXPECT_TRUE(tree2->children().empty());
}

TEST_F(ViewManagerTest, NodeDestroyed) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  // |node1| will be deleted after calling Destroy() below.
  TransportNodeId id = node1->id();
  node1->Destroy();

  std::set<TransportNodeId> nodes;
  nodes.insert(id);
  WaitForDestruction(view_manager_2(), &nodes, NULL);

  EXPECT_TRUE(view_manager_2()->tree()->children().empty());
  EXPECT_EQ(NULL, view_manager_2()->GetNodeById(id));
}

TEST_F(ViewManagerTest, ViewManagerDestroyed_CleanupNode) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  TransportNodeId id = node1->id();
  DestroyViewManager1();
  std::set<TransportNodeId> nodes;
  nodes.insert(id);
  WaitForDestruction(view_manager_2(), &nodes, NULL);

  // tree() should still be valid, since it's owned by neither connection.
  EXPECT_TRUE(view_manager_2()->tree()->children().empty());
}

TEST_F(ViewManagerTest, SetActiveView) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  View* view1 = View::Create(view_manager_1());
  node1->SetActiveView(view1);

  ViewTreeNode* node1_2 = view_manager_2()->tree()->GetChildById(node1->id());
  WaitForActiveViewToChange(node1_2);

  EXPECT_EQ(node1_2->active_view()->id(), view1->id());
}

TEST_F(ViewManagerTest, DestroyView) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  View* view1 = View::Create(view_manager_1());
  node1->SetActiveView(view1);

  ViewTreeNode* node1_2 = view_manager_2()->tree()->GetChildById(node1->id());
  WaitForActiveViewToChange(node1_2);

  TransportViewId view1_id = view1->id();
  view1->Destroy();

  std::set<TransportViewId> views;
  views.insert(view1_id);
  WaitForDestruction(view_manager_2(), NULL, &views);
  EXPECT_EQ(NULL, node1_2->active_view());
  EXPECT_EQ(NULL, view_manager_2()->GetViewById(view1_id));
}

// Destroying the connection that created a node and view should result in that
// node and view disappearing from all connections that see them.
TEST_F(ViewManagerTest, ViewManagerDestroyed_CleanupNodeAndView) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  View* view1 = View::Create(view_manager_1());
  node1->SetActiveView(view1);

  ViewTreeNode* node1_2 = view_manager_2()->tree()->GetChildById(node1->id());
  WaitForActiveViewToChange(node1_2);

  TransportNodeId node1_id = node1->id();
  TransportViewId view1_id = view1->id();

  DestroyViewManager1();
  std::set<TransportNodeId> observed_nodes;
  observed_nodes.insert(node1_id);
  std::set<TransportViewId> observed_views;
  observed_views.insert(view1_id);
  WaitForDestruction(view_manager_2(), &observed_nodes, &observed_views);

  // tree() should still be valid, since it's owned by neither connection.
  EXPECT_TRUE(view_manager_2()->tree()->children().empty());
  EXPECT_EQ(NULL, view_manager_2()->GetNodeById(node1_id));
  EXPECT_EQ(NULL, view_manager_2()->GetViewById(view1_id));
}

// This test validates the following scenario:
// -  a node originating from one connection
// -  a view originating from a second connection
// +  the connection originating the node is destroyed
// -> the view should still exist (since the second connection is live) but
//    should be disconnected from any nodes.
TEST_F(ViewManagerTest,
       ViewManagerDestroyed_CleanupNodeAndViewFromDifferentConnections) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  View* view1_2 = View::Create(view_manager_2());
  ViewTreeNode* node1_2 = view_manager_2()->tree()->GetChildById(node1->id());
  node1_2->SetActiveView(view1_2);
  WaitForActiveViewToChange(node1);

  TransportNodeId node1_id = node1->id();
  TransportViewId view1_2_id = view1_2->id();

  DestroyViewManager1();
  std::set<TransportNodeId> nodes;
  nodes.insert(node1_id);
  WaitForDestruction(view_manager_2(), &nodes, NULL);

  // tree() should still be valid, since it's owned by neither connection.
  EXPECT_TRUE(view_manager_2()->tree()->children().empty());
  // node1 was owned by the first connection, so it should be gone.
  EXPECT_EQ(NULL, view_manager_2()->GetNodeById(node1_id));
  // view1_2 was owned by the second connection, so it should still exist, but
  // disconnected from the node tree.
  View* another_view1_2 = view_manager_2()->GetViewById(view1_2_id);
  EXPECT_EQ(view1_2, another_view1_2);
  EXPECT_EQ(NULL, view1_2->node());
}

// This test verifies that it is not possible to set the active view to a view
// defined in a different connection.
// TODO(beng): write these tests for ViewTreeNode::AddChild(), RemoveChild() and
//             Contains().
TEST_F(ViewManagerTest, SetActiveViewAcrossConnection) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  View* view1_2 = View::Create(view_manager_2());
  EXPECT_DEATH(node1->SetActiveView(view1_2), "");
}

// This test verifies that a node hierarchy constructed in one connection
// becomes entirely visible to the second connection when the hierarchy is
// attached.
TEST_F(ViewManagerTest, MapSubtreeOnAttach) {
  ViewTreeNode* node1 = ViewTreeNode::Create(view_manager_1());
  ViewTreeNode* node11 = CreateNodeInParent(node1);
  View* view11 = View::Create(view_manager_1());
  node11->SetActiveView(view11);
  WaitForAllChangesToBeAcked(view_manager_1());

  // Now attach this node tree to the root & wait for it to show up in the
  // second connection.
  view_manager_1()->tree()->AddChild(node1);
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 3);

  ViewTreeNode* node11_2 = view_manager_2()->GetNodeById(node11->id());
  View* view11_2 = view_manager_2()->GetViewById(view11->id());
  EXPECT_TRUE(node11_2 != NULL);
  EXPECT_EQ(view11_2, node11_2->active_view());
}

// Verifies that bounds changes applied to a node hierarchy in one connection
// are reflected to another.
TEST_F(ViewManagerTest, SetBounds) {
  ViewTreeNode* node1 = CreateNodeInParent(view_manager_1()->tree());
  WaitForTreeSizeToMatch(view_manager_2()->tree(), 2);

  ViewTreeNode* node1_2 = view_manager_2()->GetNodeById(node1->id());
  EXPECT_EQ(node1->bounds(), node1_2->bounds());

  node1->SetBounds(gfx::Rect(0, 0, 100, 100));
  WaitForBoundsToChange(node1_2);
  EXPECT_EQ(node1->bounds(), node1_2->bounds());


}

}  // namespace view_manager
}  // namespace mojo
