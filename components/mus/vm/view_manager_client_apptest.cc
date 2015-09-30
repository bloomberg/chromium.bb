// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "components/mus/public/cpp/tests/view_manager_test_base.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "ui/mojo/geometry/geometry_util.h"

namespace mus {

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
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override {
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

// Wait until |view| has |tree_size| descendants; returns false on timeout. The
// count includes |view|. For example, if you want to wait for |view| to have
// a single child, use a |tree_size| of 2.
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
                       mojo::OrderDirection direction) override {
    DCHECK_EQ(view, view_);
    EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

// Wait until |view|'s tree size matches |tree_size|; returns false on timeout.
bool WaitForOrderChange(ViewTreeConnection* connection, View* view) {
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

struct EmbedResult {
  EmbedResult(ViewTreeConnection* connection, ConnectionSpecificId id)
      : connection(connection), connection_id(id) {}
  EmbedResult() : connection(nullptr), connection_id(0) {}

  ViewTreeConnection* connection;

  // The id supplied to the callback from OnEmbed(). Depending upon the
  // access policy this may or may not match the connection id of
  // |connection|.
  ConnectionSpecificId connection_id;
};

// These tests model synchronization of two peer connections to the view manager
// service, that are given access to some root view.

class ViewManagerTest : public ViewManagerTestBase {
 public:
  ViewManagerTest() {}

  // Embeds another version of the test app @ view. This runs a run loop until
  // a response is received, or a timeout. On success the new ViewManager is
  // returned.
  EmbedResult Embed(View* view) {
    return Embed(view, mojo::ViewTree::ACCESS_POLICY_DEFAULT);
  }

  EmbedResult Embed(View* view, uint32_t access_policy_bitmask) {
    DCHECK(!embed_details_);
    embed_details_.reset(new EmbedDetails);
    view->Embed(ConnectToApplicationAndGetViewManagerClient(),
                access_policy_bitmask,
                base::Bind(&ViewManagerTest::EmbedCallbackImpl,
                           base::Unretained(this)));
    embed_details_->waiting = true;
    if (!ViewManagerTestBase::DoRunLoopWithTimeout())
      return EmbedResult();
    const EmbedResult result(embed_details_->connection,
                             embed_details_->connection_id);
    embed_details_.reset();
    return result;
  }

  // Establishes a connection to this application and asks for a
  // ViewTreeClient.
  mojo::ViewTreeClientPtr ConnectToApplicationAndGetViewManagerClient() {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(application_impl()->url());
    scoped_ptr<mojo::ApplicationConnection> connection =
        application_impl()->ConnectToApplication(request.Pass());
    mojo::ViewTreeClientPtr client;
    connection->ConnectToService(&client);
    return client.Pass();
  }

  // ViewManagerTestBase:
  void OnEmbed(View* root) override {
    if (!embed_details_) {
      ViewManagerTestBase::OnEmbed(root);
      return;
    }

    embed_details_->connection = root->connection();
    if (embed_details_->callback_run)
      EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

 private:
  // Used to track the state of a call to view->Embed().
  struct EmbedDetails {
    EmbedDetails()
        : callback_run(false),
          result(false),
          waiting(false),
          connection_id(0),
          connection(nullptr) {}

    // The callback supplied to Embed() was received.
    bool callback_run;

    // The boolean supplied to the Embed() callback.
    bool result;

    // Whether a MessageLoop is running.
    bool waiting;

    // Connection id supplied to the Embed() callback.
    ConnectionSpecificId connection_id;

    // The ViewTreeConnection that resulted from the Embed(). null if |result|
    // is false.
    ViewTreeConnection* connection;
  };

  void EmbedCallbackImpl(bool result, ConnectionSpecificId connection_id) {
    embed_details_->callback_run = true;
    embed_details_->result = result;
    embed_details_->connection_id = connection_id;
    if (embed_details_->waiting && (!result || embed_details_->connection))
      EXPECT_TRUE(ViewManagerTestBase::QuitRunLoop());
  }

  scoped_ptr<EmbedDetails> embed_details_;

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
  ViewTreeConnection* embedded = Embed(view).connection;
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

  ViewTreeConnection* embedded = Embed(view).connection;
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
  ViewTreeConnection* embedded = Embed(view).connection;
  ASSERT_NE(nullptr, embedded);

  View* view_in_embedded = embedded->GetViewById(view->id());
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());

  mojo::Rect rect;
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
  ViewTreeConnection* embedded = Embed(view).connection;
  ASSERT_NE(nullptr, embedded);

  View* view_in_embedded = embedded->GetViewById(view->id());
  mojo::Rect rect;
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
  ViewTreeConnection* embedded = Embed(view).connection;
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
  ViewTreeConnection* embedded1 = Embed(view1).connection;
  ASSERT_NE(nullptr, embedded1);
  ViewTreeConnection* embedded2 = Embed(view2).connection;
  ASSERT_NE(nullptr, embedded2);
  EXPECT_NE(embedded1, embedded2);
}

// TODO(alhaad): Currently, the RunLoop gets stuck waiting for order change.
// Debug and re-enable this.
TEST_F(ViewManagerTest, DISABLED_Reorder) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  ViewTreeConnection* embedded = Embed(view1).connection;
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
    ASSERT_TRUE(
        WaitForOrderChange(embedded, embedded->GetViewById(view11->id())));

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
  ViewTreeConnection* embedded = Embed(view1).connection;
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
  ViewTreeConnection* embedded = Embed(view1).connection;
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
    EXPECT_TRUE(!gained_focus || gained_focus->HasFocus());
    EXPECT_FALSE(lost_focus && lost_focus->HasFocus());
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

  ViewTreeConnection* embedded = Embed(view1).connection;
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
  {
    // Add an observer on the View that loses focus, and make sure the observer
    // sees the right values.
    FocusChangeObserver observer(view11);
    embedded->GetRoot()->SetFocus();
    ASSERT_TRUE(DoRunLoopWithTimeout());
    ASSERT_NE(nullptr, observer.last_gained_focus());
    ASSERT_NE(nullptr, observer.last_lost_focus());
    EXPECT_EQ(view11->id(), observer.last_lost_focus()->id());
    EXPECT_EQ(embedded->GetRoot()->id(), observer.last_gained_focus()->id());
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

    // We should always get OnViewDestroyed() before OnConnectionLost().
    EXPECT_FALSE(test_->view_tree_connection_destroyed());
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
  ViewTreeConnection* connection = Embed(view).connection;
  ASSERT_TRUE(connection);
  bool got_destroy = false;
  DestroyedChangedObserver observer(this, connection->GetRoot(), &got_destroy);
  delete connection;
  EXPECT_TRUE(view_tree_connection_destroyed());
  EXPECT_TRUE(got_destroy);
}

// Verifies two Embed()s in the same view trigger deletion of the first
// ViewManager.
TEST_F(ViewManagerTest, DisconnectTriggersDelete) {
  View* view = window_manager()->CreateView();
  ASSERT_NE(nullptr, view);
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewTreeConnection* connection = Embed(view).connection;
  EXPECT_NE(connection, window_manager());
  View* embedded_view = connection->CreateView();
  // Embed again, this should trigger disconnect and deletion of connection.
  bool got_destroy;
  DestroyedChangedObserver observer(this, embedded_view, &got_destroy);
  EXPECT_FALSE(view_tree_connection_destroyed());
  Embed(view);
  EXPECT_TRUE(view_tree_connection_destroyed());
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
  view1->Embed(ConnectToApplicationAndGetViewManagerClient());
  EXPECT_TRUE(observer.was_removed());
  EXPECT_EQ(nullptr, view2->parent());
  EXPECT_TRUE(view1->children().empty());

  // Run the message loop so the Embed() call above completes. Without this
  // we may end up reconnecting to the test and rerunning the test, which is
  // problematic since the other services don't shut down.
  ASSERT_TRUE(DoRunLoopWithTimeout());
}

namespace {

class DestroyObserver : public ViewObserver {
 public:
  DestroyObserver(ViewManagerTestBase* test,
                  ViewTreeConnection* connection,
                  bool* got_destroy)
      : test_(test), got_destroy_(got_destroy) {
    connection->GetRoot()->AddObserver(this);
  }
  ~DestroyObserver() override {}

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    *got_destroy_ = true;
    view->RemoveObserver(this);

    // We should always get OnViewDestroyed() before OnViewManagerDestroyed().
    EXPECT_FALSE(test_->view_tree_connection_destroyed());

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

  ViewTreeConnection* embedded_connection = Embed(embed_view).connection;

  bool got_destroy = false;
  DestroyObserver observer(this, embedded_connection, &got_destroy);
  // Delete the view |embedded_connection| is embedded in. This is async,
  // but will eventually trigger deleting |embedded_connection|.
  embed_view->Destroy();
  EXPECT_TRUE(DoRunLoopWithTimeout());
  EXPECT_TRUE(got_destroy);
}

// Verifies an embed root sees views created beneath it from another
// connection.
TEST_F(ViewManagerTest, EmbedRootSeesHierarchyChanged) {
  View* embed_view = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(embed_view);

  ViewTreeConnection* vm2 =
      Embed(embed_view, mojo::ViewTree::ACCESS_POLICY_EMBED_ROOT).connection;
  View* vm2_v1 = vm2->CreateView();
  vm2->GetRoot()->AddChild(vm2_v1);

  ViewTreeConnection* vm3 = Embed(vm2_v1).connection;
  View* vm3_v1 = vm3->CreateView();
  vm3->GetRoot()->AddChild(vm3_v1);

  // As |vm2| is an embed root it should get notified about |vm3_v1|.
  ASSERT_TRUE(WaitForTreeSizeToMatch(vm2_v1, 2));
}

TEST_F(ViewManagerTest, EmbedFromEmbedRoot) {
  View* embed_view = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(embed_view);

  // Give the connection embedded at |embed_view| embed root powers.
  const EmbedResult result1 =
      Embed(embed_view, mojo::ViewTree::ACCESS_POLICY_EMBED_ROOT);
  ViewTreeConnection* vm2 = result1.connection;
  EXPECT_EQ(result1.connection_id, vm2->GetConnectionId());
  View* vm2_v1 = vm2->CreateView();
  vm2->GetRoot()->AddChild(vm2_v1);

  const EmbedResult result2 = Embed(vm2_v1);
  ViewTreeConnection* vm3 = result2.connection;
  EXPECT_EQ(result2.connection_id, vm3->GetConnectionId());
  View* vm3_v1 = vm3->CreateView();
  vm3->GetRoot()->AddChild(vm3_v1);

  // Embed from v3, the callback should not get the connection id as vm3 is not
  // an embed root.
  const EmbedResult result3 = Embed(vm3_v1);
  ASSERT_TRUE(result3.connection);
  EXPECT_EQ(0, result3.connection_id);

  // As |vm2| is an embed root it should get notified about |vm3_v1|.
  ASSERT_TRUE(WaitForTreeSizeToMatch(vm2_v1, 2));

  // Embed() from vm2 in vm3_v1. This is allowed as vm2 is an embed root, and
  // further the callback should see the connection id.
  ASSERT_EQ(1u, vm2_v1->children().size());
  View* vm3_v1_in_vm2 = vm2_v1->children()[0];
  const EmbedResult result4 = Embed(vm3_v1_in_vm2);
  ASSERT_TRUE(result4.connection);
  EXPECT_EQ(result4.connection_id, result4.connection->GetConnectionId());
}

}  // namespace mus
