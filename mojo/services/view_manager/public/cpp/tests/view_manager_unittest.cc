// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/public/cpp/view_manager.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/geometry/public/cpp/geometry_util.h"
#include "mojo/services/view_manager/public/cpp/lib/view_manager_client_impl.h"
#include "mojo/services/view_manager/public/cpp/lib/view_private.h"
#include "mojo/services/view_manager/public/cpp/util.h"
#include "mojo/services/view_manager/public/cpp/view_manager_client_factory.h"
#include "mojo/services/view_manager/public/cpp/view_manager_delegate.h"
#include "mojo/services/view_manager/public/cpp/view_observer.h"
#include "shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

const char kWindowManagerURL[] = "mojo:window_manager";
const char kEmbeddedApp1URL[] = "mojo:embedded_app_1";

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

class ConnectApplicationLoader : public ApplicationLoader,
                                 public ApplicationDelegate,
                                 public ViewManagerDelegate {
 public:
  typedef base::Callback<void(ViewManager*, View*)> LoadedCallback;

  explicit ConnectApplicationLoader(const LoadedCallback& callback)
      : callback_(callback) {}
  ~ConnectApplicationLoader() override {}

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    view_manager_client_factory_.reset(
        new ViewManagerClientFactory(app->shell(), this));
  }

  // Overridden from ApplicationLoader:
  void Load(ApplicationManager* manager,
            const GURL& url,
            ScopedMessagePipeHandle shell_handle,
            LoadCallback callback) override {
    ASSERT_TRUE(shell_handle.is_valid());
    scoped_ptr<ApplicationImpl> app(
        new ApplicationImpl(this, shell_handle.Pass()));
    apps_.push_back(app.release());
  }

  void OnApplicationError(ApplicationManager* manager,
                          const GURL& url) override {}

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  // Overridden from ViewManagerDelegate:
  void OnEmbed(ViewManager* view_manager,
               View* root,
               ServiceProviderImpl* exported_services,
               scoped_ptr<ServiceProvider> imported_services) override {
    callback_.Run(view_manager, root);
  }
  void OnViewManagerDisconnected(ViewManager* view_manager) override {}

  ScopedVector<ApplicationImpl> apps_;
  LoadedCallback callback_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectApplicationLoader);
};

class BoundsChangeObserver : public ViewObserver {
 public:
  explicit BoundsChangeObserver(View* view) : view_(view) {}
  ~BoundsChangeObserver() override {}

 private:
  // Overridden from ViewObserver:
  void OnViewBoundsChanged(View* view,
                           const Rect& old_bounds,
                           const Rect& new_bounds) override {
    DCHECK_EQ(view, view_);
    QuitRunLoop();
  }

  View* view_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

// Wait until the bounds of the supplied view change.
void WaitForBoundsToChange(View* view) {
  BoundsChangeObserver observer(view);
  view->AddObserver(&observer);
  DoRunLoop();
  view->RemoveObserver(&observer);
}

// Spins a runloop until the tree beginning at |root| has |tree_size| views
// (including |root|).
class TreeSizeMatchesObserver : public ViewObserver {
 public:
  TreeSizeMatchesObserver(View* tree, size_t tree_size)
      : tree_(tree),
        tree_size_(tree_size) {}
  ~TreeSizeMatchesObserver() override {}

  bool IsTreeCorrectSize() {
    return CountViews(tree_) == tree_size_;
  }

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

  DISALLOW_COPY_AND_ASSIGN(TreeSizeMatchesObserver);
};

void WaitForTreeSizeToMatch(View* view, size_t tree_size) {
  TreeSizeMatchesObserver observer(view, tree_size);
  if (observer.IsTreeCorrectSize())
    return;
  view->AddObserver(&observer);
  DoRunLoop();
  view->RemoveObserver(&observer);
}

// Utility class that waits for the destruction of some number of views and
// views.
class DestructionObserver : public ViewObserver {
 public:
  // |views| or |views| can be NULL.
  explicit DestructionObserver(std::set<Id>* views) : views_(views) {}

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    std::set<Id>::iterator it = views_->find(view->id());
    if (it != views_->end())
      views_->erase(it);
    if (CanQuit())
      QuitRunLoop();
  }

  bool CanQuit() {
    return !views_ || views_->empty();
  }

  std::set<Id>* views_;

  DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
};

void WaitForDestruction(ViewManager* view_manager, std::set<Id>* views) {
  DestructionObserver observer(views);
  DCHECK(views);
  if (views) {
    for (std::set<Id>::const_iterator it = views->begin();
          it != views->end(); ++it) {
      view_manager->GetViewById(*it)->AddObserver(&observer);
    }
  }
  DoRunLoop();
}

class OrderChangeObserver : public ViewObserver {
 public:
  OrderChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
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

  DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

void WaitForOrderChange(ViewManager* view_manager, View* view) {
  OrderChangeObserver observer(view);
  DoRunLoop();
}

// Tracks a view's destruction. Query is_valid() for current state.
class ViewTracker : public ViewObserver {
 public:
  explicit ViewTracker(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~ViewTracker() override {
    if (view_)
      view_->RemoveObserver(this);
  }

  bool is_valid() const { return !!view_; }

 private:
  // Overridden from ViewObserver:
  void OnViewDestroyed(View* view) override {
    DCHECK_EQ(view, view_);
    view_ = NULL;
  }

  int id_;
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewTracker);
};

}  // namespace

// ViewManager -----------------------------------------------------------------

// These tests model synchronization of two peer connections to the view manager
// service, that are given access to some root view.

class ViewManagerTest : public testing::Test {
 public:
  ViewManagerTest()
      : connect_loop_(NULL),
        loaded_view_manager_(NULL),
        window_manager_(NULL),
        commit_count_(0) {}

 protected:
  ViewManager* window_manager() { return window_manager_; }

  View* CreateViewInParent(View* parent) {
    ViewManager* parent_manager = ViewPrivate(parent).view_manager();
    View* view = View::Create(parent_manager);
    view->SetVisible(true);
    parent->AddChild(view);
    return view;
  }

  // Embeds another version of the test app @ view.
  ViewManager* Embed(ViewManager* view_manager, View* view) {
    DCHECK_EQ(view_manager, ViewPrivate(view).view_manager());
    view->Embed(kEmbeddedApp1URL);
    RunRunLoop();
    return GetLoadedViewManager();
  }

  ViewManager* GetLoadedViewManager() {
    ViewManager* view_manager = loaded_view_manager_;
    loaded_view_manager_ = NULL;
    return view_manager;
  }

  void UnloadApplication(const GURL& url) {
    test_helper_.SetLoaderForURL(scoped_ptr<ApplicationLoader>(), url);
  }

 private:
  // Overridden from testing::Test:
  void SetUp() override {
    ConnectApplicationLoader::LoadedCallback ready_callback = base::Bind(
        &ViewManagerTest::OnViewManagerLoaded, base::Unretained(this));
    test_helper_.Init();
    test_helper_.SetLoaderForURL(
        scoped_ptr<ApplicationLoader>(
            new ConnectApplicationLoader(ready_callback)),
        GURL(kWindowManagerURL));
    test_helper_.SetLoaderForURL(
        scoped_ptr<ApplicationLoader>(
            new ConnectApplicationLoader(ready_callback)),
        GURL(kEmbeddedApp1URL));

    // TODO(sky): resolve this. Need to establish initial connection.
  }

  void OnViewManagerLoaded(ViewManager* view_manager, View* root) {
    loaded_view_manager_ = view_manager;
    connect_loop_->Quit();
  }

  void RunRunLoop() {
    base::RunLoop run_loop;
    connect_loop_ = &run_loop;
    connect_loop_->Run();
    connect_loop_ = NULL;
  }

  base::RunLoop* connect_loop_;
  shell::ShellTestHelper test_helper_;
  // Used to receive the most recent view manager loaded by an embed action.
  ViewManager* loaded_view_manager_;
  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewManager* window_manager_;
  int commit_count_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

// TODO(sky): all of these tests are disabled as each test triggers running
// ViewsInit, which tries to register the same set of paths with the
// PathService, triggering a DCHECK.
TEST_F(ViewManagerTest, DISABLED_SetUp) {}

TEST_F(ViewManagerTest, DISABLED_Embed) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);
  EXPECT_TRUE(NULL != embedded);

  View* view_in_embedded = embedded->GetRoot();
  EXPECT_EQ(view->parent(), window_manager()->GetRoot());
  EXPECT_EQ(NULL, view_in_embedded->parent());
}

// Window manager has two views, N1 and N11. Embeds A at N1. A should not see
// N11.
// TODO(sky): Update client lib to match server.
TEST_F(ViewManagerTest, DISABLED_EmbeddedDoesntSeeChild) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  View* nested = View::Create(window_manager());
  nested->SetVisible(true);
  view->AddChild(nested);

  ViewManager* embedded = Embed(window_manager(), view);
  EXPECT_EQ(embedded->GetRoot()->children().front()->id(),
            nested->id());
  EXPECT_TRUE(embedded->GetRoot()->children().empty());
  EXPECT_TRUE(nested->parent() == NULL);
}

// http://crbug.com/396300
TEST_F(ViewManagerTest, DISABLED_ViewManagerDestroyed_CleanupView) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);

  Id view_id = view->id();

  UnloadApplication(GURL(kWindowManagerURL));

  std::set<Id> views;
  views.insert(view_id);
  WaitForDestruction(embedded, &views);

  EXPECT_EQ(nullptr, embedded->GetRoot());
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
TEST_F(ViewManagerTest, DISABLED_SetBounds) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);

  View* view_in_embedded = embedded->GetViewById(view->id());
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());

  Rect rect;
  rect.width = rect.height = 100;
  view->SetBounds(rect);
  EXPECT_NE(view->bounds(), view_in_embedded->bounds());
  WaitForBoundsToChange(view_in_embedded);
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());
}

// Verifies that bounds changes applied to a view owned by a different
// connection are refused.
TEST_F(ViewManagerTest, DISABLED_SetBoundsSecurity) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);

  View* view_in_embedded = embedded->GetViewById(view->id());
  Rect rect;
  rect.width = 800;
  rect.height = 600;
  view->SetBounds(rect);
  WaitForBoundsToChange(view_in_embedded);

  rect.width = 1024;
  rect.height = 768;
  view_in_embedded->SetBounds(rect);
  // Bounds change should have been rejected.
  EXPECT_EQ(view->bounds(), view_in_embedded->bounds());
}

// Verifies that a view can only be destroyed by the connection that created it.
TEST_F(ViewManagerTest, DISABLED_DestroySecurity) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);

  View* view_in_embedded = embedded->GetViewById(view->id());

  ViewTracker tracker2(view_in_embedded);
  view_in_embedded->Destroy();
  // View should not have been destroyed.
  EXPECT_TRUE(tracker2.is_valid());

  ViewTracker tracker1(view);
  view->Destroy();
  EXPECT_FALSE(tracker1.is_valid());
}

TEST_F(ViewManagerTest, DISABLED_MultiRoots) {
  View* view1 = View::Create(window_manager());
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);
  View* view2 = View::Create(window_manager());
  view2->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view2);
  ViewManager* embedded1 = Embed(window_manager(), view1);
  ViewManager* embedded2 = Embed(window_manager(), view2);
  EXPECT_EQ(embedded1, embedded2);
}

TEST_F(ViewManagerTest, DISABLED_EmbeddingIdentity) {
  View* view = View::Create(window_manager());
  view->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view);
  ViewManager* embedded = Embed(window_manager(), view);
  EXPECT_EQ(kWindowManagerURL, embedded->GetEmbedderURL());
}

TEST_F(ViewManagerTest, DISABLED_Reorder) {
  View* view1 = View::Create(window_manager());
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  ViewManager* embedded = Embed(window_manager(), view1);

  View* view11 = View::Create(embedded);
  view11->SetVisible(true);
  embedded->GetRoot()->AddChild(view11);
  View* view12 = View::Create(embedded);
  view12->SetVisible(true);
  embedded->GetRoot()->AddChild(view12);

  View* view1_in_wm = window_manager()->GetViewById(view1->id());

  {
    WaitForTreeSizeToMatch(view1, 2u);
    view11->MoveToFront();
    WaitForOrderChange(window_manager(),
                       window_manager()->GetViewById(view11->id()));

    EXPECT_EQ(view1_in_wm->children().front(),
              window_manager()->GetViewById(view12->id()));
    EXPECT_EQ(view1_in_wm->children().back(),
              window_manager()->GetViewById(view11->id()));
  }

  {
    view11->MoveToBack();
    WaitForOrderChange(window_manager(),
                       window_manager()->GetViewById(view11->id()));

    EXPECT_EQ(view1_in_wm->children().front(),
              window_manager()->GetViewById(view11->id()));
    EXPECT_EQ(view1_in_wm->children().back(),
              window_manager()->GetViewById(view12->id()));
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

  DISALLOW_COPY_AND_ASSIGN(VisibilityChangeObserver);
};

}  // namespace

TEST_F(ViewManagerTest, DISABLED_Visible) {
  View* view1 = View::Create(window_manager());
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  // Embed another app and verify initial state.
  ViewManager* embedded = Embed(window_manager(), view1);
  ASSERT_NE(nullptr, embedded->GetRoot());
  View* embedded_root = embedded->GetRoot();
  EXPECT_TRUE(embedded_root->visible());
  EXPECT_TRUE(embedded_root->IsDrawn());

  // Change the visible state from the first connection and verify its mirrored
  // correctly to the embedded app.
  {
    VisibilityChangeObserver observer(embedded_root);
    view1->SetVisible(false);
    DoRunLoop();
  }

  EXPECT_FALSE(view1->visible());
  EXPECT_FALSE(view1->IsDrawn());

  EXPECT_FALSE(embedded_root->visible());
  EXPECT_FALSE(embedded_root->IsDrawn());

  // Make the node visible again.
  {
    VisibilityChangeObserver observer(embedded_root);
    view1->SetVisible(true);
    DoRunLoop();
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

  DISALLOW_COPY_AND_ASSIGN(DrawnChangeObserver);
};

}  // namespace

TEST_F(ViewManagerTest, DISABLED_Drawn) {
  View* view1 = View::Create(window_manager());
  view1->SetVisible(true);
  window_manager()->GetRoot()->AddChild(view1);

  // Embed another app and verify initial state.
  ViewManager* embedded = Embed(window_manager(), view1);
  ASSERT_NE(nullptr, embedded->GetRoot());
  View* embedded_root = embedded->GetRoot();
  EXPECT_TRUE(embedded_root->visible());
  EXPECT_TRUE(embedded_root->IsDrawn());

  // Change the visibility of the root, this should propagate a drawn state
  // change to |embedded|.
  {
    DrawnChangeObserver observer(embedded_root);
    window_manager()->GetRoot()->SetVisible(false);
    DoRunLoop();
  }

  EXPECT_TRUE(view1->visible());
  EXPECT_FALSE(view1->IsDrawn());

  EXPECT_TRUE(embedded_root->visible());
  EXPECT_FALSE(embedded_root->IsDrawn());
}

// TODO(beng): tests for view event dispatcher.
// - verify that we see events for all views.

// TODO(beng): tests for focus:
// - focus between two views known to a connection
// - focus between views unknown to one of the connections.
// - focus between views unknown to either connection.

// TODO(sky): need test of root being destroyed with existing views. See
// 434555 for specific case.

}  // namespace mojo
