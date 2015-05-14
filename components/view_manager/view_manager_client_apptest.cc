// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/view_manager/public/cpp/lib/view_manager_client_impl.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_context.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mojo/application/application_test_base_chromium.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "ui/mojo/geometry/geometry_util.h"

namespace mojo {

namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

bool DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
      TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

void QuitRunLoop() {
  current_run_loop->Quit();
  current_run_loop = nullptr;
}

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
    QuitRunLoop();
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

// Wait until the bounds of the supplied view change; returns false on timeout.
bool WaitForBoundsToChange(View* view) {
  BoundsChangeObserver observer(view);
  return DoRunLoopWithTimeout();
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
      QuitRunLoop();
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
  return observer.IsTreeCorrectSize() || DoRunLoopWithTimeout();
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
    QuitRunLoop();
  }

  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

// Wait until |view|'s tree size matches |tree_size|; returns false on timeout.
bool WaitForOrderChange(ViewManager* view_manager, View* view) {
  OrderChangeObserver observer(view);
  return DoRunLoopWithTimeout();
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

  int id_;
  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewTracker);
};

}  // namespace

// ViewManager -----------------------------------------------------------------

// These tests model synchronization of two peer connections to the view manager
// service, that are given access to some root view.

class ViewManagerTest : public test::ApplicationTestBase,
                        public ApplicationDelegate,
                        public ViewManagerDelegate {
 public:
  ViewManagerTest()
      : most_recent_view_manager_(nullptr), window_manager_(nullptr) {}

  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
  }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  ViewManager* window_manager() { return window_manager_; }

  // Embeds another version of the test app @ view; returns nullptr on timeout.
  ViewManager* Embed(ViewManager* view_manager, View* view) {
    DCHECK_EQ(view_manager, view->view_manager());
    most_recent_view_manager_ = nullptr;
    view->Embed(application_impl()->url());
    if (!DoRunLoopWithTimeout())
      return nullptr;
    ViewManager* vm = nullptr;
    std::swap(vm, most_recent_view_manager_);
    return vm;
  }

  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // Overridden from ViewManagerDelegate:
  void OnEmbed(View* root,
               InterfaceRequest<ServiceProvider> services,
               ServiceProviderPtr exposed_services) override {
    most_recent_view_manager_ = root->view_manager();
    QuitRunLoop();
  }
  void OnViewManagerDisconnected(ViewManager* view_manager) override {}

 private:
  // Overridden from testing::Test:
  void SetUp() override {
    ApplicationTestBase::SetUp();

    view_manager_context_.reset(new ViewManagerContext(application_impl()));
    view_manager_context_->Embed(application_impl()->url());
    ASSERT_TRUE(DoRunLoopWithTimeout());
    std::swap(window_manager_, most_recent_view_manager_);
  }

  // Overridden from testing::Test:
  void TearDown() override { ApplicationTestBase::TearDown(); }

  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  scoped_ptr<ViewManagerContext> view_manager_context_;

  // Used to receive the most recent view manager loaded by an embed action.
  ViewManager* most_recent_view_manager_;
  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewManager* window_manager_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

TEST_F(ViewManagerTest, RootView) {
  ASSERT_NE(nullptr, window_manager());
  EXPECT_NE(nullptr, window_manager()->GetRoot());
  EXPECT_EQ("mojo:window_manager", window_manager()->GetEmbedderURL());
}

TEST_F(ViewManagerTest, Embed) {
  View* view = window_manager()->CreateView();
  ASSERT_NE(nullptr, view);
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);
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

  ViewManager* embedded = Embed(window_manager(), view);
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
  ViewManager* embedded = Embed(window_manager(), view);
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
  ViewManager* embedded = Embed(window_manager(), view);
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
  ViewManager* embedded = Embed(window_manager(), view);
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
  ViewManager* embedded1 = Embed(window_manager(), view1);
  ASSERT_NE(nullptr, embedded1);
  ViewManager* embedded2 = Embed(window_manager(), view2);
  ASSERT_NE(nullptr, embedded2);
  EXPECT_NE(embedded1, embedded2);
}

TEST_F(ViewManagerTest, EmbeddingIdentity) {
  View* view = window_manager()->CreateView();
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);
  ASSERT_NE(nullptr, embedded);
  EXPECT_EQ(application_impl()->url(), embedded->GetEmbedderURL());
}

// TODO(alhaad): Currently, the RunLoop gets stuck waiting for order change.
// Debug and re-enable this.
TEST_F(ViewManagerTest, DISABLED_Reorder) {
  View* view1 = window_manager()->CreateView();
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* embedded = Embed(window_manager(), view1);
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
    QuitRunLoop();
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
  ViewManager* embedded = Embed(window_manager(), view1);
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
    ASSERT_TRUE(DoRunLoopWithTimeout());
  }

  EXPECT_FALSE(view1->visible());
  EXPECT_FALSE(view1->IsDrawn());

  EXPECT_FALSE(embedded_root->visible());
  EXPECT_FALSE(embedded_root->IsDrawn());

  // Make the node visible again.
  {
    VisibilityChangeObserver observer(embedded_root);
    view1->SetVisible(true);
    ASSERT_TRUE(DoRunLoopWithTimeout());
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
    QuitRunLoop();
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
  ViewManager* embedded = Embed(window_manager(), view1);
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
    QuitRunLoop();
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

  ViewManager* embedded = Embed(window_manager(), view1);
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

}  // namespace mojo
