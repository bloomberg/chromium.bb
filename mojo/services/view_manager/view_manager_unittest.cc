// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/test_change_tracker.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_WIN)
#include "ui/gfx/win/window_impl.h"
#endif

namespace mojo {
namespace service {

namespace {

const char kTestServiceURL[] = "mojo:test_url";
const char kTestServiceURL2[] = "mojo:test_url2";

// ViewManagerProxy is a proxy to an ViewManagerService. It handles invoking
// ViewManagerService functions on the right thread in a synchronous manner
// (each ViewManagerService cover function blocks until the response from the
// ViewManagerService is returned). In addition it tracks the set of
// ViewManagerClient messages received by way of a vector of Changes. Use
// DoRunLoopUntilChangesCount() to wait for a certain number of messages to be
// received.
class ViewManagerProxy : public TestChangeTracker::Delegate {
 public:
  explicit ViewManagerProxy(TestChangeTracker* tracker)
      : tracker_(tracker),
        main_loop_(NULL),
        view_manager_(NULL),
        quit_count_(0),
        router_(NULL) {
    SetInstance(this);
  }

  virtual ~ViewManagerProxy() {
  }

  // Returns true if in an initial state. If this returns false it means the
  // last test didn't clean up properly, or most likely didn't invoke
  // WaitForInstance() when it needed to.
  static bool IsInInitialState() { return instance_ == NULL; }

  // Runs a message loop until the single instance has been created.
  static ViewManagerProxy* WaitForInstance() {
    if (!instance_)
      RunMainLoop();
    ViewManagerProxy* instance = instance_;
    instance_ = NULL;
    return instance;
  }

  ViewManagerService* view_manager() { return view_manager_; }

  // Runs the main loop until |count| changes have been received.
  std::vector<Change> DoRunLoopUntilChangesCount(size_t count) {
    DCHECK_EQ(0u, quit_count_);
    if (tracker_->changes()->size() >= count) {
      CopyChangesFromTracker();
      return changes_;
    }
    quit_count_ = count - tracker_->changes()->size();
    // Run the current message loop. When |count| Changes have been received,
    // we'll quit.
    RunMainLoop();
    return changes_;
  }

  const std::vector<Change>& changes() const { return changes_; }

  // Destroys the connection, blocking until done.
  void Destroy() {
    router_->CloseMessagePipe();
  }

  void ClearChanges() {
    changes_.clear();
    tracker_->changes()->clear();
  }

  void CopyChangesFromTracker() {
    std::vector<Change> changes;
    tracker_->changes()->swap(changes);
    changes_.swap(changes);
  }

  // The following functions are cover methods for ViewManagerService. They
  // block until the result is received.
  bool CreateView(Id view_id) {
    changes_.clear();
    ErrorCode result = ERROR_CODE_NONE;
    view_manager_->CreateView(
        view_id,
        base::Bind(&ViewManagerProxy::GotResultWithErrorCode,
                   base::Unretained(this),
                   &result));
    RunMainLoop();
    return result == ERROR_CODE_NONE;
  }
  ErrorCode CreateViewWithErrorCode(Id view_id) {
    changes_.clear();
    ErrorCode result = ERROR_CODE_NONE;
    view_manager_->CreateView(
        view_id,
        base::Bind(&ViewManagerProxy::GotResultWithErrorCode,
                   base::Unretained(this),
                   &result));
    RunMainLoop();
    return result;
  }
  bool AddView(Id parent, Id child) {
    changes_.clear();
    bool result = false;
    view_manager_->AddView(parent, child,
                           base::Bind(&ViewManagerProxy::GotResult,
                                      base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }
  bool RemoveViewFromParent(Id view_id) {
    changes_.clear();
    bool result = false;
    view_manager_->RemoveViewFromParent(
        view_id,
        base::Bind(
            &ViewManagerProxy::GotResult, base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }
  bool ReorderView(Id view_id, Id relative_view_id, OrderDirection direction) {
    changes_.clear();
    bool result = false;
    view_manager_->ReorderView(
        view_id,
        relative_view_id,
        direction,
        base::Bind(
            &ViewManagerProxy::GotResult, base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }
  void GetViewTree(Id view_id, std::vector<TestView>* views) {
    changes_.clear();
    view_manager_->GetViewTree(
        view_id,
        base::Bind(
            &ViewManagerProxy::GotViewTree, base::Unretained(this), views));
    RunMainLoop();
  }
  bool Embed(const Id view_id, const char* url) {
    changes_.clear();
    base::AutoReset<bool> auto_reset(&in_embed_, true);
    bool result = false;
    ServiceProviderPtr services;
    view_manager_->Embed(
        url,
        view_id,
        services.Pass(),
        base::Bind(
            &ViewManagerProxy::GotResult, base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }
  bool DeleteView(Id view_id) {
    changes_.clear();
    bool result = false;
    view_manager_->DeleteView(
        view_id,
        base::Bind(
            &ViewManagerProxy::GotResult, base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }
  bool SetViewBounds(Id view_id, const gfx::Rect& bounds) {
    changes_.clear();
    bool result = false;
    view_manager_->SetViewBounds(
        view_id,
        Rect::From(bounds),
        base::Bind(
            &ViewManagerProxy::GotResult, base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }
  bool SetViewVisibility(Id view_id, bool visible) {
    changes_.clear();
    bool result = false;
    view_manager_->SetViewVisibility(
        view_id,
        visible,
        base::Bind(
            &ViewManagerProxy::GotResult, base::Unretained(this), &result));
    RunMainLoop();
    return result;
  }

 private:
  friend class TestViewManagerClientConnection;

  void set_router(mojo::internal::Router* router) { router_ = router; }

  void set_view_manager(ViewManagerService* view_manager) {
    view_manager_ = view_manager;
  }

  static void RunMainLoop() {
    DCHECK(!main_run_loop_);
    main_run_loop_ = new base::RunLoop;
    main_run_loop_->Run();
    delete main_run_loop_;
    main_run_loop_ = NULL;
  }

  void QuitCountReached() {
    CopyChangesFromTracker();
    main_run_loop_->Quit();
  }

  static void SetInstance(ViewManagerProxy* instance) {
    DCHECK(!instance_);
    instance_ = instance;
    // Embed() runs its own run loop that is quit when the result is
    // received. Embed() also results in a new instance. If we quit here while
    // waiting for a Embed() we would prematurely return before we got the
    // result from Embed().
    if (!in_embed_ && main_run_loop_)
      main_run_loop_->Quit();
  }

  // Callbacks from the various ViewManagerService functions.
  void GotResult(bool* result_cache, bool result) {
    *result_cache = result;
    DCHECK(main_run_loop_);
    main_run_loop_->Quit();
  }

  void GotResultWithErrorCode(ErrorCode* error_code_cache,
                              ErrorCode error_code) {
    *error_code_cache = error_code;
    DCHECK(main_run_loop_);
    main_run_loop_->Quit();
  }

  void GotViewTree(std::vector<TestView>* views, Array<ViewDataPtr> results) {
    ViewDatasToTestViews(results, views);
    DCHECK(main_run_loop_);
    main_run_loop_->Quit();
  }

  // TestChangeTracker::Delegate:
  virtual void OnChangeAdded() OVERRIDE {
    if (quit_count_ > 0 && --quit_count_ == 0)
      QuitCountReached();
  }

  static ViewManagerProxy* instance_;
  static base::RunLoop* main_run_loop_;
  static bool in_embed_;

  TestChangeTracker* tracker_;

  // MessageLoop of the test.
  base::MessageLoop* main_loop_;

  ViewManagerService* view_manager_;

  // Number of changes we're waiting on until we quit the current loop.
  size_t quit_count_;

  std::vector<Change> changes_;

  mojo::internal::Router* router_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerProxy);
};

// static
ViewManagerProxy* ViewManagerProxy::instance_ = NULL;

// static
base::RunLoop* ViewManagerProxy::main_run_loop_ = NULL;

// static
bool ViewManagerProxy::in_embed_ = false;

class TestViewManagerClientConnection
    : public InterfaceImpl<ViewManagerClient> {
 public:
  TestViewManagerClientConnection() : connection_(&tracker_) {
    tracker_.set_delegate(&connection_);
  }

  // InterfaceImpl:
  virtual void OnConnectionEstablished() OVERRIDE {
    connection_.set_router(internal_state()->router());
    connection_.set_view_manager(client());
  }

  // ViewManagerClient:
  virtual void OnEmbed(
      ConnectionSpecificId connection_id,
      const String& creator_url,
      ViewDataPtr root,
      InterfaceRequest<ServiceProvider> services) OVERRIDE {
    tracker_.OnEmbed(connection_id, creator_url, root.Pass());
  }
  virtual void OnViewBoundsChanged(Id view_id,
                                   RectPtr old_bounds,
                                   RectPtr new_bounds) OVERRIDE {
    tracker_.OnViewBoundsChanged(view_id, old_bounds.Pass(), new_bounds.Pass());
  }
  virtual void OnViewHierarchyChanged(Id view,
                                      Id new_parent,
                                      Id old_parent,
                                      Array<ViewDataPtr> views) OVERRIDE {
    tracker_.OnViewHierarchyChanged(view, new_parent, old_parent, views.Pass());
  }
  virtual void OnViewReordered(Id view_id,
                               Id relative_view_id,
                               OrderDirection direction) OVERRIDE {
    tracker_.OnViewReordered(view_id, relative_view_id, direction);
  }
  virtual void OnViewDeleted(Id view) OVERRIDE { tracker_.OnViewDeleted(view); }
  virtual void OnViewVisibilityChanged(uint32_t view, bool visible) OVERRIDE {
    tracker_.OnViewVisibilityChanged(view, visible);
  }
  virtual void OnViewDrawnStateChanged(uint32_t view, bool drawn) OVERRIDE {
    tracker_.OnViewDrawnStateChanged(view, drawn);
  }
  virtual void OnViewInputEvent(Id view_id,
                                EventPtr event,
                                const Callback<void()>& callback) OVERRIDE {
    tracker_.OnViewInputEvent(view_id, event.Pass());
  }
  virtual void Embed(
      const String& url,
      InterfaceRequest<ServiceProvider> service_provider) OVERRIDE {
    tracker_.DelegateEmbed(url);
  }
  virtual void DispatchOnViewInputEvent(mojo::EventPtr event) OVERRIDE {
  }

 private:
  TestChangeTracker tracker_;
  ViewManagerProxy connection_;

  DISALLOW_COPY_AND_ASSIGN(TestViewManagerClientConnection);
};

// Used with ViewManagerService::Embed(). Creates a
// TestViewManagerClientConnection, which creates and owns the ViewManagerProxy.
class EmbedApplicationLoader : public ApplicationLoader,
                               ApplicationDelegate,
                               public InterfaceFactory<ViewManagerClient> {
 public:
  EmbedApplicationLoader() {}
  virtual ~EmbedApplicationLoader() {}

  // ApplicationLoader implementation:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE {
    ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
    if (!shell_handle.is_valid())
      return;
    scoped_ptr<ApplicationImpl> app(new ApplicationImpl(this,
                                                        shell_handle.Pass()));
    apps_.push_back(app.release());
  }
  virtual void OnApplicationError(ApplicationManager* manager,
                                  const GURL& url) OVERRIDE {}

  // ApplicationDelegate implementation:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory<ViewManagerClient> implementation:
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<ViewManagerClient> request) OVERRIDE {
    BindToRequest(new TestViewManagerClientConnection, &request);
  }

 private:
  ScopedVector<ApplicationImpl> apps_;

  DISALLOW_COPY_AND_ASSIGN(EmbedApplicationLoader);
};

// Creates an id used for transport from the specified parameters.
Id BuildViewId(ConnectionSpecificId connection_id,
               ConnectionSpecificId view_id) {
  return (connection_id << 16) | view_id;
}

// Callback from Embed(). |result| is the result of the
// Embed() call and |run_loop| the nested RunLoop.
void EmbedCallback(bool* result_cache, base::RunLoop* run_loop, bool result) {
  *result_cache = result;
  run_loop->Quit();
}

// Embed from an application that does not yet have a view manager connection.
// Blocks until result is determined.
bool InitEmbed(ViewManagerInitService* view_manager_init,
               const std::string& url,
               size_t number_of_calls) {
  bool result = false;
  base::RunLoop run_loop;
  for (size_t i = 0; i < number_of_calls; ++i) {
    ServiceProviderPtr sp;
    view_manager_init->Embed(url, sp.Pass(),
                             base::Bind(&EmbedCallback, &result, &run_loop));
  }
  run_loop.Run();
  return result;
}

}  // namespace

typedef std::vector<std::string> Changes;

class ViewManagerTest : public testing::Test {
 public:
  ViewManagerTest()
      : connection_(NULL),
        connection2_(NULL),
        connection3_(NULL) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(ViewManagerProxy::IsInInitialState());
    test_helper_.Init();

#if defined(OS_WIN)
    // As we unload the wndproc of window classes we need to be sure to
    // unregister them.
    gfx::WindowImpl::UnregisterClassesAtExit();
#endif

    test_helper_.SetLoaderForURL(
        scoped_ptr<ApplicationLoader>(new EmbedApplicationLoader()),
        GURL(kTestServiceURL));

    test_helper_.SetLoaderForURL(
        scoped_ptr<ApplicationLoader>(new EmbedApplicationLoader()),
        GURL(kTestServiceURL2));

    test_helper_.application_manager()->ConnectToService(
        GURL("mojo:mojo_view_manager"), &view_manager_init_);
    ASSERT_TRUE(InitEmbed(view_manager_init_.get(), kTestServiceURL, 1));

    connection_ = ViewManagerProxy::WaitForInstance();
    ASSERT_TRUE(connection_ != NULL);
    connection_->DoRunLoopUntilChangesCount(1);
  }

  virtual void TearDown() OVERRIDE {
    if (connection3_)
      connection3_->Destroy();
    if (connection2_)
      connection2_->Destroy();
    if (connection_)
      connection_->Destroy();
  }

 protected:
  void EstablishSecondConnectionWithRoot(Id root_id) {
    ASSERT_TRUE(connection_->Embed(root_id, kTestServiceURL));
    connection2_ = ViewManagerProxy::WaitForInstance();
    ASSERT_TRUE(connection2_ != NULL);
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
  }

  // Creates a second connection to the viewmanager.
  void EstablishSecondConnection(bool create_initial_view) {
    if (create_initial_view)
      ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
    ASSERT_NO_FATAL_FAILURE(
        EstablishSecondConnectionWithRoot(BuildViewId(1, 1)));
    const std::vector<Change>& changes(connection2_->changes());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnEmbed creator=mojo:test_url",
              ChangesToDescription1(changes)[0]);
    if (create_initial_view)
      EXPECT_EQ("[view=1,1 parent=null]", ChangeViewDescription(changes));
  }

  void EstablishThirdConnection(ViewManagerProxy* owner, Id root_id) {
    ASSERT_TRUE(connection3_ == NULL);
    ASSERT_TRUE(owner->Embed(root_id, kTestServiceURL2));
    connection3_ = ViewManagerProxy::WaitForInstance();
    ASSERT_TRUE(connection3_ != NULL);
    connection3_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection3_->changes().size());
    EXPECT_EQ("OnEmbed creator=mojo:test_url",
              ChangesToDescription1(connection3_->changes())[0]);
  }

  void DestroySecondConnection() {
    connection2_->Destroy();
    connection2_ = NULL;
  }

  base::ShadowingAtExitManager at_exit_;
  shell::ShellTestHelper test_helper_;

  ViewManagerInitServicePtr view_manager_init_;

  // NOTE: this connection is the root. As such, it has special permissions.
  ViewManagerProxy* connection_;
  ViewManagerProxy* connection2_;
  ViewManagerProxy* connection3_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerTest);
};

TEST_F(ViewManagerTest, SecondEmbedRoot_InitService) {
  ASSERT_TRUE(InitEmbed(view_manager_init_.get(), kTestServiceURL, 1));
  connection_->DoRunLoopUntilChangesCount(1);
  EXPECT_EQ(kTestServiceURL, connection_->changes()[0].embed_url);
}

TEST_F(ViewManagerTest, SecondEmbedRoot_Service) {
  ASSERT_TRUE(connection_->Embed(BuildViewId(0, 0), kTestServiceURL));
  connection_->DoRunLoopUntilChangesCount(1);
  EXPECT_EQ(kTestServiceURL, connection_->changes()[0].embed_url);
}

TEST_F(ViewManagerTest, MultipleEmbedRootsBeforeWTHReady) {
  ASSERT_TRUE(InitEmbed(view_manager_init_.get(), kTestServiceURL, 2));
  connection_->DoRunLoopUntilChangesCount(2);
  EXPECT_EQ(kTestServiceURL, connection_->changes()[0].embed_url);
  EXPECT_EQ(kTestServiceURL, connection_->changes()[1].embed_url);
}

// Verifies client gets a valid id.
// http://crbug.com/396492
TEST_F(ViewManagerTest, DISABLED_ValidId) {
  // TODO(beng): this should really have the URL of the application that
  //             connected to ViewManagerInit.
  EXPECT_EQ("OnEmbed creator=",
            ChangesToDescription1(connection_->changes())[0]);

  // All these tests assume 1 for the client id. The only real assertion here is
  // the client id is not zero, but adding this as rest of code here assumes 1.
  EXPECT_EQ(1, connection_->changes()[0].connection_id);
}

// Verifies two clients/connections get different ids.
TEST_F(ViewManagerTest, TwoClientsGetDifferentConnectionIds) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_EQ("OnEmbed creator=mojo:test_url",
            ChangesToDescription1(connection2_->changes())[0]);

  // It isn't strictly necessary that the second connection gets 2, but these
  // tests are written assuming that is the case. The key thing is the
  // connection ids of |connection_| and |connection2_| differ.
  EXPECT_EQ(2, connection2_->changes()[0].connection_id);
}

// Verifies when Embed() is invoked any child views are removed.
TEST_F(ViewManagerTest, ViewsRemovedWhenEmbedding) {
  // Two views 1 and 2. 2 is parented to 1.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 1), BuildViewId(1, 2)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  EXPECT_EQ("[view=1,1 parent=null]",
            ChangeViewDescription(connection2_->changes()));

  // Embed() removed view 2.
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(1, 2), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=1,2 parent=null", views[0].ToString());
  }

  // |connection2_| should not see view 2.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=1,1 parent=null", views[0].ToString());
  }
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(1, 2), &views);
    EXPECT_TRUE(views.empty());
  }

  // Views 3 and 4 in connection 2.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 3)));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 4)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(2, 3), BuildViewId(2, 4)));

  // Connection 3 rooted at 2.
  ASSERT_NO_FATAL_FAILURE(
      EstablishThirdConnection(connection2_, BuildViewId(2, 3)));

  // View 4 should no longer have a parent.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(2, 3), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=2,3 parent=null", views[0].ToString());

    views.clear();
    connection2_->GetViewTree(BuildViewId(2, 4), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=2,4 parent=null", views[0].ToString());
  }

  // And view 4 should not be visible to connection 3.
  {
    std::vector<TestView> views;
    connection3_->GetViewTree(BuildViewId(2, 3), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=2,3 parent=null", views[0].ToString());
  }
}

// Verifies once Embed() has been invoked the parent connection can't see any
// children.
TEST_F(ViewManagerTest, CantAccessChildrenOfEmbeddedView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));

  ASSERT_NO_FATAL_FAILURE(
      EstablishThirdConnection(connection2_, BuildViewId(2, 2)));

  ASSERT_TRUE(connection3_->CreateView(BuildViewId(3, 3)));
  ASSERT_TRUE(connection3_->AddView(BuildViewId(2, 2), BuildViewId(3, 3)));

  // Even though 3 is a child of 2 connection 2 can't see 3 as it's from a
  // different connection.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(2, 2), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=2,2 parent=1,1", views[0].ToString());
  }

  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(3, 3), &views);
    EXPECT_TRUE(views.empty());
  }

  // Connection 2 shouldn't be able to get view 3 at all.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(3, 3), &views);
    EXPECT_TRUE(views.empty());
  }

  // Connection 1 should be able to see it all (its the root).
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(3u, views.size());
    EXPECT_EQ("view=1,1 parent=null", views[0].ToString());
    EXPECT_EQ("view=2,2 parent=1,1", views[1].ToString());
    EXPECT_EQ("view=3,3 parent=2,2", views[2].ToString());
  }
}

// Verifies once Embed() has been invoked the parent can't mutate the children.
TEST_F(ViewManagerTest, CantModifyChildrenOfEmbeddedView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));

  ASSERT_NO_FATAL_FAILURE(
      EstablishThirdConnection(connection2_, BuildViewId(2, 2)));

  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 3)));
  // Connection 2 shouldn't be able to add anything to the view anymore.
  ASSERT_FALSE(connection2_->AddView(BuildViewId(2, 2), BuildViewId(2, 3)));

  // Create view 3 in connection 3 and add it to view 3.
  ASSERT_TRUE(connection3_->CreateView(BuildViewId(3, 3)));
  ASSERT_TRUE(connection3_->AddView(BuildViewId(2, 2), BuildViewId(3, 3)));

  // Connection 2 shouldn't be able to remove view 3.
  ASSERT_FALSE(connection2_->RemoveViewFromParent(BuildViewId(3, 3)));
}

// Verifies client gets a valid id.
TEST_F(ViewManagerTest, CreateView) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  EXPECT_TRUE(connection_->changes().empty());

  // Can't create a view with the same id.
  ASSERT_EQ(ERROR_CODE_VALUE_IN_USE,
            connection_->CreateViewWithErrorCode(BuildViewId(1, 1)));
  EXPECT_TRUE(connection_->changes().empty());

  // Can't create a view with a bogus connection id.
  EXPECT_EQ(ERROR_CODE_ILLEGAL_ARGUMENT,
            connection_->CreateViewWithErrorCode(BuildViewId(2, 1)));
  EXPECT_TRUE(connection_->changes().empty());
}

// Verifies AddView fails when view is already in position.
TEST_F(ViewManagerTest, AddViewWithNoChange) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 2), BuildViewId(1, 3)));

  // Try again, this should fail.
  EXPECT_FALSE(connection_->AddView(BuildViewId(1, 2), BuildViewId(1, 3)));
}

// Verifies AddView fails when view is already in position.
TEST_F(ViewManagerTest, AddAncestorFails) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 2), BuildViewId(1, 3)));

  // Try to make 2 a child of 3, this should fail since 2 is an ancestor of 3.
  EXPECT_FALSE(connection_->AddView(BuildViewId(1, 3), BuildViewId(1, 2)));
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerTest, AddToRoot) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 21)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 21.
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 21), BuildViewId(1, 3)));

  // Make 21 a child of 1.
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 1), BuildViewId(1, 21)));

  // Connection 2 should not be told anything (because the view is from a
  // different connection). Create a view to ensure we got a response from
  // the server.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 100)));
  connection2_->CopyChangesFromTracker();
  EXPECT_TRUE(connection2_->changes().empty());
}

// Verifies HierarchyChanged is correctly sent for various adds/removes.
TEST_F(ViewManagerTest, ViewHierarchyChangedViews) {
  // 1,2->1,11.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 11)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 2), BuildViewId(1, 11)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // 1,1->1,2->1,11
  {
    // Client 2 should not get anything (1,2 is from another connection).
    connection2_->ClearChanges();
    ASSERT_TRUE(connection_->AddView(BuildViewId(1, 1), BuildViewId(1, 2)));
    ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 100)));
    connection2_->CopyChangesFromTracker();
    EXPECT_TRUE(connection2_->changes().empty());
  }

  // 0,1->1,1->1,2->1,11.
  {
    // Client 2 is now connected to the root, so it should have gotten a drawn
    // notification.
    ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,1 drawn=true",
              ChangesToDescription1(connection2_->changes())[0]);
  }

  // 1,1->1,2->1,11.
  {
    // Client 2 is no longer connected to the root, should get drawn state
    // changed.
    ASSERT_TRUE(connection_->RemoveViewFromParent(BuildViewId(1, 1)));
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,1 drawn=false",
              ChangesToDescription1(connection2_->changes())[0]);
  }

  // 1,1->1,2->1,11->1,111.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 111)));
  {
    connection2_->ClearChanges();
    ASSERT_TRUE(connection_->AddView(BuildViewId(1, 11), BuildViewId(1, 111)));
    ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 103)));
    connection2_->CopyChangesFromTracker();
    EXPECT_TRUE(connection2_->changes().empty());
  }

  // 0,1->1,1->1,2->1,11->1,111
  {
    connection2_->ClearChanges();
    ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,1 drawn=true",
              ChangesToDescription1(connection2_->changes())[0]);
  }
}

TEST_F(ViewManagerTest, ViewHierarchyChangedAddingKnownToUnknown) {
  // Create the following structure: root -> 1 -> 11 and 2->21 (2 has no
  // parent).
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 11)));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 21)));

  // Set up the hierarchy.
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 11)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(2, 2), BuildViewId(2, 21)));

  // Remove 11, should result in a hierarchy change for the root.
  {
    connection_->ClearChanges();
    ASSERT_TRUE(connection2_->RemoveViewFromParent(BuildViewId(2, 11)));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("HierarchyChanged view=2,11 new_parent=null old_parent=1,1",
              changes[0]);
  }

  // Add 2 to 1.
  {
    ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("HierarchyChanged view=2,2 new_parent=1,1 old_parent=null",
              changes[0]);
    EXPECT_EQ(
        "[view=2,2 parent=1,1],"
        "[view=2,21 parent=2,2]",
        ChangeViewDescription(connection_->changes()));
  }
}

TEST_F(ViewManagerTest, ReorderView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id view1_id = BuildViewId(2, 1);
  Id view2_id = BuildViewId(2, 2);
  Id view3_id = BuildViewId(2, 3);
  Id view4_id = BuildViewId(1, 4);  // Peer to 1,1
  Id view5_id = BuildViewId(1, 5);  // Peer to 1,1
  Id view6_id = BuildViewId(2, 6);  // Child of 1,2.
  Id view7_id = BuildViewId(2, 7);  // Unparented.
  Id view8_id = BuildViewId(2, 8);  // Unparented.
  ASSERT_TRUE(connection2_->CreateView(view1_id));
  ASSERT_TRUE(connection2_->CreateView(view2_id));
  ASSERT_TRUE(connection2_->CreateView(view3_id));
  ASSERT_TRUE(connection_->CreateView(view4_id));
  ASSERT_TRUE(connection_->CreateView(view5_id));
  ASSERT_TRUE(connection2_->CreateView(view6_id));
  ASSERT_TRUE(connection2_->CreateView(view7_id));
  ASSERT_TRUE(connection2_->CreateView(view8_id));
  ASSERT_TRUE(connection2_->AddView(view1_id, view2_id));
  ASSERT_TRUE(connection2_->AddView(view2_id, view6_id));
  ASSERT_TRUE(connection2_->AddView(view1_id, view3_id));
  ASSERT_TRUE(
      connection_->AddView(ViewIdToTransportId(RootViewId()), view4_id));
  ASSERT_TRUE(
      connection_->AddView(ViewIdToTransportId(RootViewId()), view5_id));

  ASSERT_TRUE(
      connection_->AddView(ViewIdToTransportId(RootViewId()), view1_id));

  {
    ASSERT_TRUE(
        connection2_->ReorderView(view2_id, view3_id, ORDER_DIRECTION_ABOVE));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("Reordered view=2,2 relative=2,3 direction=above", changes[0]);
  }

  {
    ASSERT_TRUE(
        connection2_->ReorderView(view2_id, view3_id, ORDER_DIRECTION_BELOW));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("Reordered view=2,2 relative=2,3 direction=below", changes[0]);
  }

  // view2 is already below view3.
  EXPECT_FALSE(
      connection2_->ReorderView(view2_id, view3_id, ORDER_DIRECTION_BELOW));

  // view4 & 5 are unknown to connection2_.
  EXPECT_FALSE(
      connection2_->ReorderView(view4_id, view5_id, ORDER_DIRECTION_ABOVE));

  // view6 & view3 have different parents.
  EXPECT_FALSE(
      connection_->ReorderView(view3_id, view6_id, ORDER_DIRECTION_ABOVE));

  // Non-existent view-ids
  EXPECT_FALSE(connection_->ReorderView(
      BuildViewId(1, 27), BuildViewId(1, 28), ORDER_DIRECTION_ABOVE));

  // view7 & view8 are un-parented.
  EXPECT_FALSE(
      connection_->ReorderView(view7_id, view8_id, ORDER_DIRECTION_ABOVE));
}

// Verifies DeleteView works.
TEST_F(ViewManagerTest, DeleteView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));
    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("HierarchyChanged view=2,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }

  // Delete 2.
  {
    ASSERT_TRUE(connection2_->DeleteView(BuildViewId(2, 2)));
    EXPECT_TRUE(connection2_->changes().empty());

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewDeleted view=2,2", changes[0]);
  }
}

// Verifies DeleteView isn't allowed from a separate connection.
TEST_F(ViewManagerTest, DeleteViewFromAnotherConnectionDisallowed) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_FALSE(connection2_->DeleteView(BuildViewId(1, 1)));
}

// Verifies if a view was deleted and then reused that other clients are
// properly notified.
TEST_F(ViewManagerTest, ReuseDeletedViewId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));

  // Add 2 to 1.
  {
    ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    EXPECT_EQ("HierarchyChanged view=2,2 new_parent=1,1 old_parent=null",
              changes[0]);
    EXPECT_EQ("[view=2,2 parent=1,1]",
              ChangeViewDescription(connection_->changes()));
  }

  // Delete 2.
  {
    ASSERT_TRUE(connection2_->DeleteView(BuildViewId(2, 2)));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewDeleted view=2,2", changes[0]);
  }

  // Create 2 again, and add it back to 1. Should get the same notification.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));
  {
    ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));

    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    EXPECT_EQ("HierarchyChanged view=2,2 new_parent=1,1 old_parent=null",
              changes[0]);
    EXPECT_EQ("[view=2,2 parent=1,1]",
              ChangeViewDescription(connection_->changes()));
  }
}

// Assertions for GetViewTree.
TEST_F(ViewManagerTest, GetViewTree) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Create 11 in first connection and make it a child of 1.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 11)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 1), BuildViewId(1, 11)));

  // Create two views in second connection, 2 and 3, both children of 1.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 2)));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 3)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 2)));
  ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(2, 3)));

  // Verifies GetViewTree() on the root. The root connection sees all.
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(0, 1), &views);
    ASSERT_EQ(5u, views.size());
    EXPECT_EQ("view=0,1 parent=null", views[0].ToString());
    EXPECT_EQ("view=1,1 parent=0,1", views[1].ToString());
    EXPECT_EQ("view=1,11 parent=1,1", views[2].ToString());
    EXPECT_EQ("view=2,2 parent=1,1", views[3].ToString());
    EXPECT_EQ("view=2,3 parent=1,1", views[4].ToString());
  }

  // Verifies GetViewTree() on the view 1,1. This does not include any children
  // as they are not from this connection.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=1,1 parent=null", views[0].ToString());
  }

  // Connection 2 shouldn't be able to get the root tree.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(0, 1), &views);
    ASSERT_EQ(0u, views.size());
  }
}

TEST_F(ViewManagerTest, SetViewBounds) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  ASSERT_TRUE(
      connection_->SetViewBounds(BuildViewId(1, 1), gfx::Rect(0, 0, 100, 100)));

  connection2_->DoRunLoopUntilChangesCount(1);
  const Changes changes(ChangesToDescription1(connection2_->changes()));
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ("BoundsChanged view=1,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100",
            changes[0]);

  // Should not be possible to change the bounds of a view created by another
  // connection.
  ASSERT_FALSE(
      connection2_->SetViewBounds(BuildViewId(1, 1), gfx::Rect(0, 0, 0, 0)));
}

// Verify AddView fails when trying to manipulate views in other roots.
TEST_F(ViewManagerTest, CantMoveViewsFromOtherRoot) {
  // Create 1 and 2 in the first connection.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Try to move 2 to be a child of 1 from connection 2. This should fail as 2
  // should not be able to access 1.
  ASSERT_FALSE(connection2_->AddView(BuildViewId(1, 1), BuildViewId(1, 2)));

  // Try to reparent 1 to the root. A connection is not allowed to reparent its
  // roots.
  ASSERT_FALSE(connection2_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
}

// Verify RemoveViewFromParent fails for views that are descendants of the
// roots.
TEST_F(ViewManagerTest, CantRemoveViewsInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));

  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 2)));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Connection 2 should not be able to remove view 2 or 1 from its parent.
  ASSERT_FALSE(connection2_->RemoveViewFromParent(BuildViewId(1, 2)));
  ASSERT_FALSE(connection2_->RemoveViewFromParent(BuildViewId(1, 1)));

  // Create views 10 and 11 in 2.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 10)));
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 11)));

  // Parent 11 to 10.
  ASSERT_TRUE(connection2_->AddView(BuildViewId(2, 10), BuildViewId(2, 11)));
  // Remove 11 from 10.
  ASSERT_TRUE(connection2_->RemoveViewFromParent(BuildViewId(2, 11)));

  // Verify nothing was actually removed.
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(0, 1), &views);
    ASSERT_EQ(3u, views.size());
    EXPECT_EQ("view=0,1 parent=null", views[0].ToString());
    EXPECT_EQ("view=1,1 parent=0,1", views[1].ToString());
    EXPECT_EQ("view=1,2 parent=0,1", views[2].ToString());
  }
}

// Verify GetViewTree fails for views that are not descendants of the roots.
TEST_F(ViewManagerTest, CantGetViewTreeOfOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));

  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 2)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  std::vector<TestView> views;

  // Should get nothing for the root.
  connection2_->GetViewTree(BuildViewId(0, 1), &views);
  ASSERT_TRUE(views.empty());

  // Should get nothing for view 2.
  connection2_->GetViewTree(BuildViewId(1, 2), &views);
  ASSERT_TRUE(views.empty());

  // Should get view 1 if asked for.
  connection2_->GetViewTree(BuildViewId(1, 1), &views);
  ASSERT_EQ(1u, views.size());
  EXPECT_EQ("view=1,1 parent=null", views[0].ToString());
}

TEST_F(ViewManagerTest, OnViewInput) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Dispatch an event to the view and verify its received.
  {
    EventPtr event(Event::New());
    event->action = static_cast<EventType>(1);
    connection_->view_manager()->DispatchOnViewInputEvent(BuildViewId(1, 1),
                                                          event.Pass());
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("InputEvent view=1,1 event_action=1", changes[0]);
  }
}

TEST_F(ViewManagerTest, EmbedWithSameViewId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_NO_FATAL_FAILURE(
      EstablishThirdConnection(connection_, BuildViewId(1, 1)));

  // Connection2 should have been told the view was deleted.
  {
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewDeleted view=1,1", changes[0]);
  }

  // Connection2 has no root. Verify it can't see view 1,1 anymore.
  {
    std::vector<TestView> views;
    connection2_->GetViewTree(BuildViewId(1, 1), &views);
    EXPECT_TRUE(views.empty());
  }
}

TEST_F(ViewManagerTest, EmbedWithSameViewId2) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_NO_FATAL_FAILURE(
      EstablishThirdConnection(connection_, BuildViewId(1, 1)));

  // Connection2 should have been told the view was deleted.
  connection2_->DoRunLoopUntilChangesCount(1);
  connection2_->ClearChanges();

  // Create a view in the third connection and parent it to the root.
  ASSERT_TRUE(connection3_->CreateView(BuildViewId(3, 1)));
  ASSERT_TRUE(connection3_->AddView(BuildViewId(1, 1), BuildViewId(3, 1)));

  // Connection 1 should have been told about the add (it owns the view).
  {
    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("HierarchyChanged view=3,1 new_parent=1,1 old_parent=null",
              changes[0]);
  }

  // Embed 1,1 again.
  {
    // We should get a new connection for the new embedding.
    ASSERT_TRUE(connection_->Embed(BuildViewId(1, 1), kTestServiceURL));
    ViewManagerProxy* connection4 = ViewManagerProxy::WaitForInstance();
    connection4->DoRunLoopUntilChangesCount(1);
    const std::vector<Change>& changes(connection4->changes());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnEmbed creator=mojo:test_url",
              ChangesToDescription1(changes)[0]);
    EXPECT_EQ("[view=1,1 parent=null]", ChangeViewDescription(changes));

    // And 3 should get a delete.
    connection3_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection3_->changes().size());
    EXPECT_EQ("ViewDeleted view=1,1",
              ChangesToDescription1(connection3_->changes())[0]);
  }

  // Connection3_ has no root. Verify it can't see view 1,1 anymore.
  {
    std::vector<TestView> views;
    connection3_->GetViewTree(BuildViewId(1, 1), &views);
    EXPECT_TRUE(views.empty());
  }

  // Verify 3,1 is no longer parented to 1,1. We have to do this from 1,1 as
  // connection3_ can no longer see 1,1.
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=1,1 parent=null", views[0].ToString());
  }

  // Verify connection3_ can still see the view it created 3,1.
  {
    std::vector<TestView> views;
    connection3_->GetViewTree(BuildViewId(3, 1), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=3,1 parent=null", views[0].ToString());
  }
}

// Assertions for SetViewVisibility.
TEST_F(ViewManagerTest, SetViewVisibility) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));

  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(0, 1), &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ("view=0,1 parent=null visible=true drawn=true",
              views[0].ToString2());
    EXPECT_EQ("view=1,1 parent=0,1 visible=true drawn=true",
              views[1].ToString2());
  }

  // Hide 1.
  ASSERT_TRUE(connection_->SetViewVisibility(BuildViewId(1, 1), false));
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ("view=1,1 parent=0,1 visible=false drawn=false",
              views[0].ToString2());
  }

  // Attach 2 to 1.
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 1), BuildViewId(1, 2)));
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ("view=1,1 parent=0,1 visible=false drawn=false",
              views[0].ToString2());
    EXPECT_EQ("view=1,2 parent=1,1 visible=true drawn=false",
              views[1].ToString2());
  }

  // Show 1.
  ASSERT_TRUE(connection_->SetViewVisibility(BuildViewId(1, 1), true));
  {
    std::vector<TestView> views;
    connection_->GetViewTree(BuildViewId(1, 1), &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ("view=1,1 parent=0,1 visible=true drawn=true",
              views[0].ToString2());
    EXPECT_EQ("view=1,2 parent=1,1 visible=true drawn=true",
              views[1].ToString2());
  }
}

// Assertions for SetViewVisibility sending notifications.
TEST_F(ViewManagerTest, SetViewVisibilityNotifications) {
  // Create 1,1 and 1,2, 1,2 and child of 1,1 and 1,1 a child of the root.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 2)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  ASSERT_TRUE(connection_->AddView(BuildViewId(1, 1), BuildViewId(1, 2)));

  // Establish the second connection at 1,2.
  ASSERT_NO_FATAL_FAILURE(
      EstablishSecondConnectionWithRoot(BuildViewId(1, 2)));

  // Add 2,3 as a child of 1,2.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 3)));
  connection_->ClearChanges();
  ASSERT_TRUE(connection2_->AddView(BuildViewId(1, 2), BuildViewId(2, 3)));
  connection_->DoRunLoopUntilChangesCount(1);

  // Hide 1,2 from connection 1. Connection 2 should see this.
  ASSERT_TRUE(connection_->SetViewVisibility(BuildViewId(1, 2), false));
  {
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("VisibilityChanged view=1,2 visible=false",
              ChangesToDescription1(connection2_->changes())[0]);
  }

  // Show 1,2 from connection 2, connection 1 should be notified.
  ASSERT_TRUE(connection2_->SetViewVisibility(BuildViewId(1, 2), true));
  {
    connection_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection_->changes().size());
    EXPECT_EQ("VisibilityChanged view=1,2 visible=true",
              ChangesToDescription1(connection_->changes())[0]);
  }

  // Hide 1,1, connection 2 should be told the draw state changed.
  ASSERT_TRUE(connection_->SetViewVisibility(BuildViewId(1, 1), false));
  {
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,2 drawn=false",
              ChangesToDescription1(connection2_->changes())[0]);
  }

  // Show 1,1 from connection 1. Connection 2 should see this.
  ASSERT_TRUE(connection_->SetViewVisibility(BuildViewId(1, 1), true));
  {
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,2 drawn=true",
              ChangesToDescription1(connection2_->changes())[0]);
  }

  // Change visibility of 2,3, connection 1 should see this.
  connection_->ClearChanges();
  ASSERT_TRUE(connection2_->SetViewVisibility(BuildViewId(2, 3), false));
  {
    connection_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection_->changes().size());
    EXPECT_EQ("VisibilityChanged view=2,3 visible=false",
              ChangesToDescription1(connection_->changes())[0]);
  }

  // Remove 1,1 from the root, connection 2 should see drawn state changed.
  ASSERT_TRUE(connection_->RemoveViewFromParent(BuildViewId(1, 1)));
  {
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,2 drawn=false",
              ChangesToDescription1(connection2_->changes())[0]);
  }

  // Add 1,1 back to the root, connection 2 should see drawn state changed.
  ASSERT_TRUE(connection_->AddView(BuildViewId(0, 1), BuildViewId(1, 1)));
  {
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
    EXPECT_EQ("DrawnStateChanged view=1,2 drawn=true",
              ChangesToDescription1(connection2_->changes())[0]);
  }
}

// TODO(sky): add coverage of test that destroys connections and ensures other
// connections get deletion notification.

// TODO(sky): need to better track changes to initial connection. For example,
// that SetBounsdViews/AddView and the like don't result in messages to the
// originating connection.

}  // namespace service
}  // namespace mojo
