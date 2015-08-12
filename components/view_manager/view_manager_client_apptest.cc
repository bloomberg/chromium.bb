// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_manager.h"

#include "base/logging.h"
#include "components/view_manager/public/cpp/lib/view_manager_client_impl.h"
#include "components/view_manager/public/cpp/tests/view_manager_test_base.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "ui/mojo/geometry/geometry_util.h"

namespace mojo {

namespace {

class BoundsChangeObserver : public ViewObserver {
 public:
  explicit BoundsChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~BoundsChangeObserver() override { view_->RemoveObserver(this); }

 private:
  // Overridden from ViewObserver:
  void OnViewBoundsChanged(View* view,
                           const Rect& old_bounds,
                           const Rect& new_bounds) override {
    DCHECK_EQ(view, view_);
    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

// Wait until the bounds of the supplied view change; returns false on timeout.
bool WaitForBoundsToChange(View* view) {
  BoundsChangeObserver observer(view);
  return ViewManagerTestBase::DoRunLoopWithTimeout();
}

// Increments the width of |view| and waits for a bounds change in |other_vm|s
// root.
bool IncrementWidthAndWaitForChange(View* view, ViewManager* other_vm) {
  mojo::Rect bounds = view->bounds();
  bounds.width++;
  view->SetBounds(bounds);
  View* view_in_vm = other_vm->GetRoot();
  if (view_in_vm == view || view_in_vm->id() != view->id())
    return false;
  return WaitForBoundsToChange(view_in_vm);
}

// Spins a run loop until the tree beginning at |root| has |tree_size| views
// (including |root|).
class TreeSizeMatchesObserver : public ViewObserver {
 public:
  TreeSizeMatchesObserver(View* tree, size_t tree_size)
      : tree_(tree), tree_size_(tree_size) {
    tree_->AddObserver(this);
  }
  ~TreeSizeMatchesObserver() override { tree_->RemoveObserver(this); }

  bool IsTreeCorrectSize() { return CountViews(tree_) == tree_size_; }

 private:
  // Overridden from ViewObserver:
  void OnTreeChanged(const TreeChangeParams& params) override {
    if (IsTreeCorrectSize())
      EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  size_t CountViews(const View* view) const {
    size_t count = 1;
    View::Children::const_iterator it = view->children().begin();
    for (; it != view->children().end(); ++it)
      count += CountViews(*it);
    return count;
  }

  View* tree_;
  size_t tree_size_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TreeSizeMatchesObserver);
};

// Wait until |view|'s tree size matches |tree_size|; returns false on timeout.
bool WaitForTreeSizeToMatch(View* view, size_t tree_size) {
  TreeSizeMatchesObserver observer(view, tree_size);
  return observer.IsTreeCorrectSize() ||
         ViewManagerTestBase::DoRunLoopWithTimeout();
}

class OrderChangeObserver : public ViewObserver {
 public:
  OrderChangeObserver(View* view) : view_(view) { view_->AddObserver(this); }
  ~OrderChangeObserver() override { view_->RemoveObserver(this); }

 private:
  // Overridden from ViewObserver:
  void OnViewReordered(View* view,
                       View* relative_view,
                       OrderDirection direction) override {
    DCHECK_EQ(view, view_);
    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

// Wait until |view|'s tree size matches |tree_size|; returns false on timeout.
bool WaitForOrderChange(ViewManager* view_manager, View* view) {
  OrderChangeObserver observer(view);
  return ViewManagerTestBase::DoRunLoopWithTimeout();
}

// Tracks a view's destruction. Query is_valid() for current state.
class ViewTracker : public ViewObserver {
 public:
  explicit ViewTracker(View* view) : view_(view) { view_->AddObserver(this); }
  ~ViewTracker() override {
    if (view_)
      view_->RemoveObserver(this);
  }

  bool is_valid() const { return !!view_; }

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    DCHECK_EQ(view, view_);
    view_ = nullptr;
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewTracker);
};

}  // namespace

// ViewManager -----------------------------------------------------------------

// These tests model synchronization of two peer connections to the view manager
// service, that are given access to some root view.

class ViewManagerTest : public ViewManagerTestBase {
 public:
  ViewManagerTest()
      : on_will_embed_count_(0u), on_will_embed_return_value_(true) {}

  void clear_on_will_embed_count() { on_will_embed_count_ = 0u; }
  size_t on_will_embed_count() const { return on_will_embed_count_; }

  void set_on_will_embed_return_value(bool value) {
    on_will_embed_return_value_ = value;
  }

  // Embeds another version of the test app @ view. This runs a run loop until
  // a response is received, or a timeout. On success the new ViewManager is
  // returned.
  ViewManager* Embed(View* view) {
    return EmbedImpl(view, EmbedType::NO_REEMBED);
  }

  // Same as Embed(), but uses EmbedAllowingReembed().
  ViewManager* EmbedAllowingReembed(View* view) {
    return EmbedImpl(view, EmbedType::ALLOW_REEMBED);
  }

  // Establishes a connection to this application and asks for a
  // ViewManagerClient. The ViewManagerClient is then embedded in |view|.
  // This does *not* wait for the connection to complete.
  void ConnectToApplicationAndEmbed(View* view) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(application_impl()->url());
    scoped_ptr<ApplicationConnection> connection =
        application_impl()->ConnectToApplication(request.Pass());
    mojo::ViewManagerClientPtr client;
    connection->ConnectToService(&client);
    view->Embed(client.Pass());
  }

  // Overridden from ViewManagerDelegate:
  void OnEmbedForDescendant(View* view,
                            mojo::URLRequestPtr request,
                            mojo::ViewManagerClientPtr* client) override {
    on_will_embed_count_++;
    if (on_will_embed_return_value_) {
      scoped_ptr<ApplicationConnection> connection =
          application_impl()->ConnectToApplication(request.Pass());
      connection->ConnectToService(client);
    } else {
      EXPECT_TRUE(QuitRunLoop());
    }
  }

 private:
  enum class EmbedType {
    ALLOW_REEMBED,
    NO_REEMBED,
  };

  ViewManager* EmbedImpl(View* view, EmbedType type) {
    most_recent_view_manager_ = nullptr;
    if (type == EmbedType::ALLOW_REEMBED) {
      mojo::URLRequestPtr request(mojo::URLRequest::New());
      request->url = mojo::String::From(application_impl()->url());
      view->EmbedAllowingReembed(request.Pass());
    } else {
      ConnectToApplicationAndEmbed(view);
    }
    if (!ViewManagerTestBase::DoRunLoopWithTimeout())
      return nullptr;
    ViewManager* vm = nullptr;
    std::swap(vm, most_recent_view_manager_);
    return vm;
  }

  // Number of times OnWillEmbed() has been called.
  size_t on_will_embed_count_;

  // Value OnWillEmbed() should return.
  bool on_will_embed_return_value_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

TEST_F(ViewManagerTest, RootView) {
  ASSERT_NE(nullptr, window_manager());
  EXPECT_NE(nullptr, window_manager()->GetRoot());
}

TEST_F(ViewManagerTest, Embed) {
  View* view = window_manager()->CreateView();
  ASSERT_NE(nullptr, view);
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(view);
  ASSERT_NE(nullptr, embedded);

  View* view_in_embedded = embedded->GetRoot();
  ASSERT_NE(nullptr, view_in_embedded);
  EXPECT_EQ(view->id(), view_in_embedded->id());
  EXPECT_EQ(nullptr, view_in_embedded->parent());
  EXPECT_TRUE(view_in_embedded->children().empty());
}

// Window manager has two views, N1 and N11. Embeds A at N1. A should not see
// N11.
TEST_F(ViewManagerTest, EmbeddedDoesntSeeChild) {
  View* view = window_manager()->CreateView();
  ASSERT_NE(nullptr, view);
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  View* nested = window_manager()->CreateView();
  ASSERT_NE(nullptr, nested);
  nested->SetVisible(true);
  view->AddChild(nested);

  ViewManager* embedded = Embed(view);
  ASSERT_NE(nullptr, embedded);
  View* view_in_embedded = embedded->GetRoot();
  EXPECT_EQ(view->id(), view_in_embedded->id());
  EXPECT_EQ(nullptr, view_in_embedded->parent());
  EXPECT_TRUE(view_in_embedded->children().empty());
}

// TODO(beng): write a replacement test for the one that once existed here:
// This test validates the following scenario:
// -  a view originating from one connection
// -  a view originating from a second connection
// +  the connection originating the view is destroyed
// -> the view should still exist (since the second connection is live) but
//    should be disconnected from any views.
// http://crbug.com/396300
//
// TODO(beng): The new test should validate the scenario as described above
//             except that the second connection still has a valid tree.

// Verifies that bounds changes applied to a view hierarchy in one connection
// are reflected to another.
TEST_F(ViewManagerTest, SetBounds) {
  View* view = window_manager()->CreateView();
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(view);
  ASSERT_NE(nullptr, embedded);

  View* view_in_embedded = embedded->GetViewById(view->id());
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());

  Rect rect;
  rect.width = rect.height = 100;
  view->SetBounds(rect);
  ASSERT_TRUE(WaitForBoundsToChange(view_in_embedded));
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());
}

// Verifies that bounds changes applied to a view owned by a different
// connection are refused.
TEST_F(ViewManagerTest, SetBoundsSecurity) {
  View* view = window_manager()->CreateView();
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(view);
  ASSERT_NE(nullptr, embedded);

  View* view_in_embedded = embedded->GetViewById(view->id());
  Rect rect;
  rect.width = 800;
  rect.height = 600;
  view->SetBounds(rect);
  ASSERT_TRUE(WaitForBoundsToChange(view_in_embedded));

  rect.width = 1024;
  rect.height = 768;
  view_in_embedded->SetBounds(rect);
  // Bounds change should have been rejected.
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());
}

// Verifies that a view can only be destroyed by the connection that created it.
TEST_F(ViewManagerTest, DestroySecurity) {
  View* view = window_manager()->CreateView();
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(view);
  ASSERT_NE(nullptr, embedded);

  View* view_in_embedded = embedded->GetViewById(view->id());

  ViewTracker tracker2(view_in_embedded);
  view_in_embedded->Destroy();
  // View should not have been destroyed.
  EXPECT_TRUE(tracker2.is_valid());

  ViewTracker tracker1(view);
  view->Destroy();
  EXPECT_FALSE(tracker1.is_valid());
}

TEST_F(ViewManagerTest, MultiRoots) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);
  View* view2 = window_manager()->CreateView();
  view2->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view2);
  ViewManager* embedded1 = Embed(view1);
  ASSERT_NE(nullptr, embedded1);
  ViewManager* embedded2 = Embed(view2);
  ASSERT_NE(nullptr, embedded2);
  EXPECT_NE(embedded1, embedded2);
}

// TODO(alhaad): Currently, the RunLoop gets stuck waiting for order change.
// Debug and re-enable this.
TEST_F(ViewManagerTest, DISABLED_Reorder) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* embedded = Embed(view1);
  ASSERT_NE(nullptr, embedded);

  View* view11 = embedded->CreateView();
  view11->SetVisible(true);
  embedded->GetRoot()->AddChild(view11);
  View* view12 = embedded->CreateView();
  view12->SetVisible(true);
  embedded->GetRoot()->AddChild(view12);

  View* root_in_embedded = embedded->GetRoot();

  {
    ASSERT_TRUE(WaitForTreeSizeToMatch(root_in_embedded, 3u));
    view11->MoveToFront();
    ASSERT_TRUE(WaitForOrderChange(embedded, root_in_embedded));

    EXPECT_EQ(root_in_embedded->children().front(),
              embedded->GetViewById(view12->id()));
    EXPECT_EQ(root_in_embedded->children().back(),
              embedded->GetViewById(view11->id()));
  }

  {
    view11->MoveToBack();
    ASSERT_TRUE(WaitForOrderChange(embedded,
                                   embedded->GetViewById(view11->id())));

    EXPECT_EQ(root_in_embedded->children().front(),
              embedded->GetViewById(view11->id()));
    EXPECT_EQ(root_in_embedded->children().back(),
              embedded->GetViewById(view12->id()));
  }
}

namespace {

class VisibilityChangeObserver : public ViewObserver {
 public:
  explicit VisibilityChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~VisibilityChangeObserver() override { view_->RemoveObserver(this); }

 private:
  // Overridden from ViewObserver:
  void OnViewVisibilityChanged(View* view) override {
    EXPECT_EQ(view, view_);
    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(VisibilityChangeObserver);
};

}  // namespace

TEST_F(ViewManagerTest, Visible) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  // Embed another app and verify initial state.
  ViewManager* embedded = Embed(view1);
  ASSERT_NE(nullptr, embedded);
  ASSERT_NE(nullptr, embedded->GetRoot());
  View* embedded_root = embedded->GetRoot();
  EXPECT_TRUE(embedded_root->visible());
  EXPECT_TRUE(embedded_root->IsDrawn());

  // Change the visible state from the first connection and verify its mirrored
  // correctly to the embedded app.
  {
    VisibilityChangeObserver observer(embedded_root);
    view1->SetVisible(false);
    ASSERT_TRUE(ViewManagerTestBase::DoRunLoopWithTimeout());
  }

  EXPECT_FALSE(view1->visible());
  EXPECT_FALSE(view1->IsDrawn());

  EXPECT_FALSE(embedded_root->visible());
  EXPECT_FALSE(embedded_root->IsDrawn());

  // Make the node visible again.
  {
    VisibilityChangeObserver observer(embedded_root);
    view1->SetVisible(true);
    ASSERT_TRUE(ViewManagerTestBase::DoRunLoopWithTimeout());
  }

  EXPECT_TRUE(view1->visible());
  EXPECT_TRUE(view1->IsDrawn());

  EXPECT_TRUE(embedded_root->visible());
  EXPECT_TRUE(embedded_root->IsDrawn());
}

namespace {

class DrawnChangeObserver : public ViewObserver {
 public:
  explicit DrawnChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~DrawnChangeObserver() override { view_->RemoveObserver(this); }

 private:
  // Overridden from ViewObserver:
  void OnViewDrawnChanged(View* view) override {
    EXPECT_EQ(view, view_);
    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DrawnChangeObserver);
};

}  // namespace

TEST_F(ViewManagerTest, Drawn) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  // Embed another app and verify initial state.
  ViewManager* embedded = Embed(view1);
  ASSERT_NE(nullptr, embedded);
  ASSERT_NE(nullptr, embedded->GetRoot());
  View* embedded_root = embedded->GetRoot();
  EXPECT_TRUE(embedded_root->visible());
  EXPECT_TRUE(embedded_root->IsDrawn());

  // Change the visibility of the root, this should propagate a drawn state
  // change to |embedded|.
  {
    DrawnChangeObserver observer(embedded_root);
    window_manager()->GetRoot()->SetVisible(false);
    ASSERT_TRUE(DoRunLoopWithTimeout());
  }

  EXPECT_TRUE(view1->visible());
  EXPECT_FALSE(view1->IsDrawn());

  EXPECT_TRUE(embedded_root->visible());
  EXPECT_FALSE(embedded_root->IsDrawn());
}

// TODO(beng): tests for view event dispatcher.
// - verify that we see events for all views.

namespace {

class FocusChangeObserver : public ViewObserver {
 public:
  explicit FocusChangeObserver(View* view)
      : view_(view), last_gained_focus_(nullptr), last_lost_focus_(nullptr) {
    view_->AddObserver(this);
  }
  ~FocusChangeObserver() override { view_->RemoveObserver(this); }

  View* last_gained_focus() { return last_gained_focus_; }

  View* last_lost_focus() { return last_lost_focus_; }

 private:
  // Overridden from ViewObserver.
  void OnViewFocusChanged(View* gained_focus, View* lost_focus) override {
    last_gained_focus_ = gained_focus;
    last_lost_focus_ = lost_focus;
    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  View* view_;
  View* last_gained_focus_;
  View* last_lost_focus_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(FocusChangeObserver);
};

}  // namespace

TEST_F(ViewManagerTest, Focus) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* embedded = Embed(view1);
  ASSERT_NE(nullptr, embedded);
  View* view11 = embedded->CreateView();
  view11->SetVisible(true);
  embedded->GetRoot()->AddChild(view11);

  // TODO(alhaad): Figure out why switching focus between views from different
  // connections is causing the tests to crash and add tests for that.
  {
    View* embedded_root = embedded->GetRoot();
    FocusChangeObserver observer(embedded_root);
    embedded_root->SetFocus();
    ASSERT_TRUE(DoRunLoopWithTimeout());
    ASSERT_NE(nullptr, observer.last_gained_focus());
    EXPECT_EQ(embedded_root->id(), observer.last_gained_focus()->id());
  }
  {
    FocusChangeObserver observer(view11);
    view11->SetFocus();
    ASSERT_TRUE(DoRunLoopWithTimeout());
    ASSERT_NE(nullptr, observer.last_gained_focus());
    ASSERT_NE(nullptr, observer.last_lost_focus());
    EXPECT_EQ(view11->id(), observer.last_gained_focus()->id());
    EXPECT_EQ(embedded->GetRoot()->id(), observer.last_lost_focus()->id());
  }
}

namespace {

class DestroyedChangedObserver : public ViewObserver {
 public:
  DestroyedChangedObserver(ViewManagerTestBase* test,
                           View* view,
                           bool* got_destroy)
      : test_(test), view_(view), got_destroy_(got_destroy) {
    view_->AddObserver(this);
  }
  ~DestroyedChangedObserver() override {
    if (view_)
      view_->RemoveObserver(this);
  }

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    EXPECT_EQ(view, view_);
    view_->RemoveObserver(this);
    *got_destroy_ = true;
    view_ = nullptr;

    // We should always get OnViewDestroyed() before OnViewManagerDestroyed().
    EXPECT_FALSE(test_->view_manager_destroyed());
  }

  ViewManagerTestBase* test_;
  View* view_;
  bool* got_destroy_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DestroyedChangedObserver);
};

}  // namespace

// Verifies deleting a ViewManager sends the right notifications.
TEST_F(ViewManagerTest, DeleteViewManager) {
  View* view = window_manager()->CreateView();
  ASSERT_NE(nullptr, view);
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* view_manager = Embed(view);
  ASSERT_TRUE(view_manager);
  bool got_destroy = false;
  DestroyedChangedObserver observer(this, view_manager->GetRoot(),
                                    &got_destroy);
  delete view_manager;
  EXPECT_TRUE(view_manager_destroyed());
  EXPECT_TRUE(got_destroy);
}

// Verifies two Embed()s in the same view trigger deletion of the first
// ViewManager.
TEST_F(ViewManagerTest, DisconnectTriggersDelete) {
  View* view = window_manager()->CreateView();
  ASSERT_NE(nullptr, view);
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* view_manager = Embed(view);
  EXPECT_NE(view_manager, window_manager());
  View* embedded_view = view_manager->CreateView();
  // Embed again, this should trigger disconnect and deletion of view_manager.
  bool got_destroy;
  DestroyedChangedObserver observer(this, embedded_view, &got_destroy);
  EXPECT_FALSE(view_manager_destroyed());
  Embed(view);
  EXPECT_TRUE(view_manager_destroyed());
}

class ViewRemovedFromParentObserver : public ViewObserver {
 public:
  explicit ViewRemovedFromParentObserver(View* view)
      : view_(view), was_removed_(false) {
    view_->AddObserver(this);
  }
  ~ViewRemovedFromParentObserver() override { view_->RemoveObserver(this); }

  bool was_removed() const { return was_removed_; }

 private:
  // Overridden from ViewObserver:
  void OnTreeChanged(const TreeChangeParams& params) override {
    if (params.target == view_ && !params.new_parent)
      was_removed_ = true;
  }

  View* view_;
  bool was_removed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewRemovedFromParentObserver);
};

TEST_F(ViewManagerTest, EmbedRemovesChildren) {
  View* view1 = window_manager()->CreateView();
  View* view2 = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(view1);
  view1->AddChild(view2);

  ViewRemovedFromParentObserver observer(view2);
  ConnectToApplicationAndEmbed(view1);
  EXPECT_TRUE(observer.was_removed());
  EXPECT_EQ(nullptr, view2->parent());
  EXPECT_TRUE(view1->children().empty());

  // Run the message loop so the Embed() call above completes. Without this
  // we may end up reconnecting to the test and rerunning the test, which is
  // problematic since the other services don't shut down.
  ASSERT_TRUE(DoRunLoopWithTimeout());
}

TEST_F(ViewManagerTest, OnWillEmbed) {
  window_manager()->SetEmbedRoot();

  View* view1 = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* view_manager = EmbedAllowingReembed(view1);
  View* view2 = view_manager->CreateView();
  view_manager->GetRoot()->AddChild(view2);

  EmbedAllowingReembed(view2);
  EXPECT_EQ(1u, on_will_embed_count());
}

// Verifies Embed() doesn't succeed if OnWillEmbed() returns false.
TEST_F(ViewManagerTest, OnWillEmbedFails) {
  window_manager()->SetEmbedRoot();

  View* view1 = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* view_manager = Embed(view1);
  View* view2 = view_manager->CreateView();
  view_manager->GetRoot()->AddChild(view2);

  clear_on_will_embed_count();
  set_on_will_embed_return_value(false);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = application_impl()->url();
  view2->EmbedAllowingReembed(request.Pass());

  EXPECT_TRUE(DoRunLoopWithTimeout());
  EXPECT_EQ(1u, on_will_embed_count());

  // The run loop above quits when OnWillEmbed() returns, which means it's
  // possible there is still an OnEmbed() message in flight. Sets the bounds of
  // |view1| and wait for it to the change in |view_manager|, that way we know
  // |view_manager| has processed all messages for it.
  EXPECT_TRUE(IncrementWidthAndWaitForChange(view1, view_manager));

  EXPECT_EQ(1u, on_will_embed_count());
}

// Verify an Embed() from an ancestor is not allowed.
TEST_F(ViewManagerTest, ReembedFails) {
  window_manager()->SetEmbedRoot();

  View* view1 = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* view_manager = Embed(view1);
  ASSERT_TRUE(view_manager);
  View* view2 = view_manager->CreateView();
  view_manager->GetRoot()->AddChild(view2);
  Embed(view2);

  // Try to embed in view2 from the window_manager. This should fail as the
  // Embed() didn't grab reembed.
  View* view2_in_wm = window_manager()->GetViewById(view2->id());
  ConnectToApplicationAndEmbed(view2_in_wm);

  // The Embed() call above returns immediately. To ensure the server has
  // processed it nudge the bounds and wait for it to be processed.
  EXPECT_TRUE(IncrementWidthAndWaitForChange(view1, view_manager));

  EXPECT_EQ(nullptr, most_recent_view_manager());
}

// Verify an Embed() from an ancestor is allowed if the ancestor is an embed
// root and Embed was done by way of EmbedAllowingReembed().
TEST_F(ViewManagerTest, ReembedSucceeds) {
  window_manager()->SetEmbedRoot();

  View* view1 = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* view_manager = Embed(view1);
  View* view2 = view_manager->CreateView();
  view_manager->GetRoot()->AddChild(view2);
  EmbedAllowingReembed(view2);

  View* view2_in_wm = window_manager()->GetViewById(view2->id());
  ViewManager* view_manager2 = Embed(view2_in_wm);
  ASSERT_TRUE(view_manager2);

  // The Embed() call above returns immediately. To ensure the server has
  // processed it nudge the bounds and wait for it to be processed.
  EXPECT_TRUE(IncrementWidthAndWaitForChange(view1, view_manager));

  EXPECT_EQ(nullptr, most_recent_view_manager());
}

namespace {

class DestroyObserver : public ViewObserver {
 public:
  DestroyObserver(ViewManagerTestBase* test, ViewManager* vm, bool* got_destroy)
      : test_(test), got_destroy_(got_destroy) {
    vm->GetRoot()->AddObserver(this);
  }
  ~DestroyObserver() override {}

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    *got_destroy_ = true;
    view->RemoveObserver(this);

    // We should always get OnViewDestroyed() before OnViewManagerDestroyed().
    EXPECT_FALSE(test_->view_manager_destroyed());

    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  ViewManagerTestBase* test_;
  bool* got_destroy_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DestroyObserver);
};

}  // namespace

// Verifies deleting a View that is the root of another connection notifies
// observers in the right order (OnViewDestroyed() before
// OnViewManagerDestroyed()).
TEST_F(ViewManagerTest, ViewManagerDestroyedAfterRootObserver) {
  View* embed_view = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(embed_view);

  ViewManager* embedded_view_manager = Embed(embed_view);

  bool got_destroy = false;
  DestroyObserver observer(this, embedded_view_manager, &got_destroy);
  // Delete the view |embedded_view_manager| is embedded in. This is async,
  // but will eventually trigger deleting |embedded_view_manager|.
  embed_view->Destroy();
  EXPECT_TRUE(DoRunLoopWithTimeout());
  EXPECT_TRUE(got_destroy);
}

}  // namespace mojo
