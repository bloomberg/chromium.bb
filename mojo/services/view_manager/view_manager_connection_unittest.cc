// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/test_change_tracker.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
namespace view_manager {
namespace service {

namespace {

// TODO(sky): clean this up. Should be moved to the single place its used.
base::RunLoop* current_run_loop = NULL;

const char kTestServiceURL[] = "mojo:test_url";

void INodesToTestNodes(const Array<INodePtr>& data,
                       std::vector<TestNode>* test_nodes) {
  for (size_t i = 0; i < data.size(); ++i) {
    TestNode node;
    node.parent_id = data[i]->parent_id;
    node.node_id = data[i]->node_id;
    node.view_id = data[i]->view_id;
    test_nodes->push_back(node);
  }
}

// ViewManagerProxy is a proxy to an IViewManager. It handles invoking
// IViewManager functions on the right thread in a synchronous manner (each
// IViewManager cover function blocks until the response from the IViewManager
// is returned). In addition it tracks the set of IViewManagerClient messages
// received by way of a vector of Changes. Use DoRunLoopUntilChangesCount() to
// wait for a certain number of messages to be received.
class ViewManagerProxy : public TestChangeTracker::Delegate {
 public:
  ViewManagerProxy(TestChangeTracker* tracker, base::MessageLoop* loop)
      : tracker_(tracker),
        main_loop_(loop),
        background_loop_(base::MessageLoop::current()),
        view_manager_(NULL),
        quit_count_(0),
        router_(NULL) {
    main_loop_->PostTask(FROM_HERE,
                         base::Bind(&ViewManagerProxy::SetInstance, this));
  }

  virtual ~ViewManagerProxy() {
  }

  // Runs a message loop until the single instance has been created.
  static ViewManagerProxy* WaitForInstance() {
    if (!instance_)
      RunMainLoop();
    ViewManagerProxy* instance = instance_;
    instance_ = NULL;
    return instance;
  }

  // Runs the main loop until |count| changes have been received.
  std::vector<Change> DoRunLoopUntilChangesCount(size_t count) {
    background_loop_->PostTask(FROM_HERE,
                               base::Bind(&ViewManagerProxy::SetQuitCount,
                                          base::Unretained(this), count));
    // Run the current message loop. When |count| Changes have been received,
    // we'll quit.
    RunMainLoop();
    return changes_;
  }

  const std::vector<Change>& changes() const { return changes_; }

  // Destroys the connection, blocking until done.
  void Destroy() {
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::DestroyOnBackgroundThread,
                   base::Unretained(this)));
    RunMainLoop();
  }

  // The following functions mirror that of IViewManager. They bounce the
  // function to the right thread and return the result.
  bool CreateNode(TransportNodeId node_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::CreateNodeOnBackgroundThread,
                   base::Unretained(this), node_id, &result));
    RunMainLoop();
    return result;
  }
  bool AddNode(TransportNodeId parent,
               TransportNodeId child,
               TransportChangeId server_change_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::AddNodeOnBackgroundThread,
                   base::Unretained(this), parent, child, server_change_id,
                   &result));
    RunMainLoop();
    return result;
  }
  bool RemoveNodeFromParent(TransportNodeId node_id,
                            TransportChangeId server_change_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::RemoveNodeFromParentOnBackgroundThread,
                   base::Unretained(this), node_id, server_change_id, &result));
    RunMainLoop();
    return result;
  }
  bool SetView(TransportNodeId node_id, TransportViewId view_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::SetViewOnBackgroundThread,
                   base::Unretained(this), node_id, view_id, &result));
    RunMainLoop();
    return result;
  }
  bool CreateView(TransportViewId view_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::CreateViewOnBackgroundThread,
                   base::Unretained(this), view_id, &result));
    RunMainLoop();
    return result;
  }
  void GetNodeTree(TransportNodeId node_id, std::vector<TestNode>* nodes) {
    changes_.clear();
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::GetNodeTreeOnBackgroundThread,
                   base::Unretained(this), node_id, nodes));
    RunMainLoop();
  }
  bool Connect(const std::vector<TransportNodeId>& nodes) {
    changes_.clear();
    base::AutoReset<bool> auto_reset(&in_connect_, true);
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::ConnectOnBackgroundThread,
                   base::Unretained(this), nodes, &result));
    RunMainLoop();
    return result;
  }
  bool DeleteNode(TransportNodeId node_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::DeleteNodeOnBackgroundThread,
                   base::Unretained(this), node_id, &result));
    RunMainLoop();
    return result;
  }
  bool DeleteView(TransportViewId node_id) {
    changes_.clear();
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::DeleteViewOnBackgroundThread,
                   base::Unretained(this), node_id, &result));
    RunMainLoop();
    return result;
  }
  bool SetNodeBounds(TransportNodeId node_id, const gfx::Rect& rect) {
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::SetNodeBoundsOnBackgroundThread,
                   base::Unretained(this), node_id, rect, &result));
    RunMainLoop();
    return result;
  }

 private:
  friend class TestViewManagerClientConnection;

  void set_router(mojo::internal::Router* router) { router_ = router; }

  void set_view_manager(IViewManager* view_manager) {
    view_manager_ = view_manager;
  }

  void DestroyOnBackgroundThread() {
    router_->CloseMessagePipe();
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::QuitOnMainThread,
                   base::Unretained(this)));
  }

  void SetQuitCount(size_t count) {
    DCHECK_EQ(background_loop_, base::MessageLoop::current());
    if (tracker_->changes()->size() >= count) {
      QuitCountReached();
      return;
    }
    quit_count_ = count - tracker_->changes()->size();
  }

  static void RunMainLoop() {
    DCHECK(!main_run_loop_);
    main_run_loop_ = new base::RunLoop;
    main_run_loop_->Run();
    delete main_run_loop_;
    main_run_loop_ = NULL;
  }

  void QuitCountReached() {
    std::vector<Change> changes;
    tracker_->changes()->swap(changes);
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::QuitCountReachedOnMain,
                   base::Unretained(this), changes));
  }

  void QuitCountReachedOnMain(const std::vector<Change>& changes) {
    changes_ = changes;
    DCHECK(main_run_loop_);
    main_run_loop_->Quit();
  }

  static void SetInstance(ViewManagerProxy* instance) {
    DCHECK(!instance_);
    instance_ = instance;
    // Connect() runs its own run loop that is quit when the result is
    // received. Connect() also results in a new instance. If we quit here while
    // waiting for a Connect() we would prematurely return before we got the
    // result from Connect().
    if (!in_connect_ && main_run_loop_)
      main_run_loop_->Quit();
  }

  // Callbacks from the various IViewManager functions.
  void GotResultOnBackgroundThread(bool* result_cache, bool result) {
    *result_cache = result;
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::QuitOnMainThread,
                   base::Unretained(this)));
  }

  void GotNodeTreeOnBackgroundThread(std::vector<TestNode>* nodes,
                                     Array<INodePtr> results) {
    INodesToTestNodes(results, nodes);
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ViewManagerProxy::QuitOnMainThread,
                   base::Unretained(this)));
  }

  void QuitOnMainThread() {
    DCHECK(main_run_loop_);
    main_run_loop_->Quit();
  }

  // The following functions correspond to the IViewManager functions. These run
  // on the background thread.
  void CreateNodeOnBackgroundThread(TransportNodeId node_id, bool* result) {
    view_manager_->CreateNode(
        node_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void AddNodeOnBackgroundThread(TransportNodeId parent,
                                 TransportNodeId child,
                                 TransportChangeId server_change_id,
                                 bool* result) {
    view_manager_->AddNode(
        parent, child, server_change_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void RemoveNodeFromParentOnBackgroundThread(
      TransportNodeId node_id,
      TransportChangeId server_change_id,
      bool* result) {
    view_manager_->RemoveNodeFromParent(node_id, server_change_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void SetViewOnBackgroundThread(TransportNodeId node_id,
                                 TransportViewId view_id,
                                 bool* result) {
    view_manager_->SetView(node_id, view_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void CreateViewOnBackgroundThread(TransportViewId view_id, bool* result) {
    view_manager_->CreateView(view_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void GetNodeTreeOnBackgroundThread(TransportNodeId node_id,
                                     std::vector<TestNode>* nodes) {
    view_manager_->GetNodeTree(node_id,
        base::Bind(&ViewManagerProxy::GotNodeTreeOnBackgroundThread,
                   base::Unretained(this), nodes));
  }
  void ConnectOnBackgroundThread(const std::vector<TransportNodeId>& ids,
                                 bool* result) {
    view_manager_->Connect(kTestServiceURL,
                           Array<TransportNodeId>::From(ids),
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void DeleteNodeOnBackgroundThread(TransportNodeId node_id, bool* result) {
    view_manager_->DeleteNode(node_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void DeleteViewOnBackgroundThread(TransportViewId view_id, bool* result) {
    view_manager_->DeleteView(view_id,
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void SetNodeBoundsOnBackgroundThread(TransportNodeId node_id,
                                       const gfx::Rect& bounds,
                                       bool* result) {
    view_manager_->SetNodeBounds(node_id, Rect::From(bounds),
        base::Bind(&ViewManagerProxy::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }

  // TestChangeTracker::Delegate:
  virtual void OnChangeAdded() OVERRIDE {
    if (quit_count_ > 0 && --quit_count_ == 0)
      QuitCountReached();
  }

  static ViewManagerProxy* instance_;
  static base::RunLoop* main_run_loop_;
  static bool in_connect_;

  TestChangeTracker* tracker_;

  // MessageLoop of the test.
  base::MessageLoop* main_loop_;

  // MessageLoop ViewManagerProxy lives on.
  base::MessageLoop* background_loop_;

  IViewManager* view_manager_;

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
bool ViewManagerProxy::in_connect_ = false;

class TestViewManagerClientConnection
    : public InterfaceImpl<IViewManagerClient> {
 public:
  explicit TestViewManagerClientConnection(base::MessageLoop* loop)
      : connection_(&tracker_, loop) {
    tracker_.set_delegate(&connection_);
  }

  // InterfaceImp:
  virtual void OnConnectionEstablished() OVERRIDE {
    connection_.set_router(internal_state()->router());
    connection_.set_view_manager(client());
  }

  // IViewMangerClient:
  virtual void OnViewManagerConnectionEstablished(
      TransportConnectionId connection_id,
      TransportChangeId next_server_change_id,
      Array<INodePtr> nodes) OVERRIDE {
    tracker_.OnViewManagerConnectionEstablished(
        connection_id, next_server_change_id, nodes.Pass());
  }
  virtual void OnServerChangeIdAdvanced(
      TransportChangeId next_server_change_id) OVERRIDE {
    tracker_.OnServerChangeIdAdvanced(next_server_change_id);
  }
  virtual void OnNodeBoundsChanged(TransportNodeId node_id,
                                   RectPtr old_bounds,
                                   RectPtr new_bounds) OVERRIDE {
    tracker_.OnNodeBoundsChanged(node_id, old_bounds.Pass(), new_bounds.Pass());
  }
  virtual void OnNodeHierarchyChanged(
      TransportNodeId node,
      TransportNodeId new_parent,
      TransportNodeId old_parent,
      TransportChangeId server_change_id,
      Array<INodePtr> nodes) OVERRIDE {
    tracker_.OnNodeHierarchyChanged(node, new_parent, old_parent,
                                    server_change_id, nodes.Pass());
  }
  virtual void OnNodeDeleted(TransportNodeId node,
                             TransportChangeId server_change_id) OVERRIDE {
    tracker_.OnNodeDeleted(node, server_change_id);
  }
  virtual void OnViewDeleted(TransportViewId view) OVERRIDE {
    tracker_.OnViewDeleted(view);
  }
  virtual void OnNodeViewReplaced(TransportNodeId node,
                                  TransportViewId new_view_id,
                                  TransportViewId old_view_id) OVERRIDE {
    tracker_.OnNodeViewReplaced(node, new_view_id, old_view_id);
  }

 private:
  TestChangeTracker tracker_;
  ViewManagerProxy connection_;

  DISALLOW_COPY_AND_ASSIGN(TestViewManagerClientConnection);
};

// Used with IViewManager::Connect(). Creates a TestViewManagerClientConnection,
// which creates and owns the ViewManagerProxy.
class ConnectServiceLoader : public ServiceLoader {
 public:
  ConnectServiceLoader() : initial_loop_(base::MessageLoop::current()) {
  }
  virtual ~ConnectServiceLoader() {
  }

  // ServiceLoader:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle shell_handle) OVERRIDE {
    scoped_ptr<Application> app(new Application(shell_handle.Pass()));
    app->AddService<TestViewManagerClientConnection>(initial_loop_);
    apps_.push_back(app.release());
  }
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

 private:
  base::MessageLoop* initial_loop_;
  ScopedVector<Application> apps_;

  DISALLOW_COPY_AND_ASSIGN(ConnectServiceLoader);
};

// Sets |current_run_loop| and runs it. It is expected that someone else quits
// the loop.
void DoRunLoop() {
  DCHECK(!current_run_loop);

  base::RunLoop run_loop;
  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = NULL;
}

// Boolean callback. Sets |result_cache| to the value of |result| and quits
// the run loop.
void BooleanCallback(bool* result_cache, bool result) {
  *result_cache = result;
  current_run_loop->Quit();
}

// Creates an id used for transport from the specified parameters.
TransportNodeId BuildNodeId(TransportConnectionId connection_id,
                            TransportConnectionSpecificNodeId node_id) {
  return (connection_id << 16) | node_id;
}

// Creates an id used for transport from the specified parameters.
TransportViewId BuildViewId(TransportConnectionId connection_id,
                            TransportConnectionSpecificViewId view_id) {
  return (connection_id << 16) | view_id;
}

// Resposible for establishing  connection to the viewmanager. Blocks until get
// back result.
bool ViewManagerInitConnect(IViewManagerInit* view_manager_init,
                            const std::string& url) {
  bool result = false;
  view_manager_init->Connect(url, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

}  // namespace

typedef std::vector<std::string> Changes;

class ViewManagerConnectionTest : public testing::Test {
 public:
  ViewManagerConnectionTest() : connection_(NULL), connection2_(NULL) {}

  virtual void SetUp() OVERRIDE {
    test_helper_.Init();

    test_helper_.SetLoaderForURL(
        scoped_ptr<ServiceLoader>(new ConnectServiceLoader()),
        GURL(kTestServiceURL));

    ConnectToService(test_helper_.service_provider(),
                     "mojo:mojo_view_manager",
                     &view_manager_init_);
    ASSERT_TRUE(ViewManagerInitConnect(view_manager_init_.get(),
                                       kTestServiceURL));

    connection_ = ViewManagerProxy::WaitForInstance();
    ASSERT_TRUE(connection_ != NULL);
    connection_->DoRunLoopUntilChangesCount(1);
  }

  virtual void TearDown() OVERRIDE {
    if (connection2_)
      connection2_->Destroy();
    if (connection_)
      connection_->Destroy();
  }

 protected:
  void EstablishSecondConnectionWithRoots(
      TransportNodeId id1,
      TransportNodeId id2) {
    std::vector<TransportNodeId> node_ids;
    node_ids.push_back(id1);
    if (id2 != 0)
      node_ids.push_back(id2);
    ASSERT_TRUE(connection_->Connect(node_ids));
    connection2_ = ViewManagerProxy::WaitForInstance();
    ASSERT_TRUE(connection2_ != NULL);
    connection2_->DoRunLoopUntilChangesCount(1);
    ASSERT_EQ(1u, connection2_->changes().size());
  }

  // Creates a second connection to the viewmanager.
  void EstablishSecondConnection(bool create_initial_node) {
    if (create_initial_node)
      ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
    ASSERT_NO_FATAL_FAILURE(
        EstablishSecondConnectionWithRoots(BuildNodeId(1, 1), 0));
    const std::vector<Change>& changes(connection2_->changes());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnConnectionEstablished", ChangesToDescription1(changes)[0]);
    if (create_initial_node) {
      EXPECT_EQ("[node=1,1 parent=null view=null]",
                ChangeNodeDescription(changes));
    }
  }

  void DestroySecondConnection() {
    connection2_->Destroy();
    connection2_ = NULL;
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;

  IViewManagerInitPtr view_manager_init_;

  ViewManagerProxy* connection_;
  ViewManagerProxy* connection2_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnectionTest);
};

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, ValidId) {
  EXPECT_EQ("OnConnectionEstablished",
            ChangesToDescription1(connection_->changes())[0]);

  // All these tests assume 1 for the client id. The only real assertion here is
  // the client id is not zero, but adding this as rest of code here assumes 1.
  EXPECT_EQ(1, connection_->changes()[0].connection_id);

  // Change ids start at 1 as well.
  EXPECT_EQ(static_cast<TransportChangeId>(1),
            connection_->changes()[0].change_id);
}

// Verifies two clients/connections get different ids.
TEST_F(ViewManagerConnectionTest, TwoClientsGetDifferentConnectionIds) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_EQ("OnConnectionEstablished",
            ChangesToDescription1(connection2_->changes())[0]);

  // It isn't strickly necessary that the second connection gets 2, but these
  // tests are written assuming that is the case. The key thing is the
  // connection ids of |connection_| and |connection2_| differ.
  EXPECT_EQ(2, connection2_->changes()[0].connection_id);

  // Change ids start at 1 as well.
  EXPECT_EQ(static_cast<TransportChangeId>(1),
            connection2_->changes()[0].change_id);
}

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, CreateNode) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  EXPECT_TRUE(connection_->changes().empty());

  // Can't create a node with the same id.
  ASSERT_FALSE(connection_->CreateNode(BuildNodeId(1, 1)));
  EXPECT_TRUE(connection_->changes().empty());

  // Can't create a node with a bogus connection id.
  EXPECT_FALSE(connection_->CreateNode(BuildNodeId(2, 1)));
  EXPECT_TRUE(connection_->changes().empty());
}

TEST_F(ViewManagerConnectionTest, CreateViewFailsWithBogusConnectionId) {
  EXPECT_FALSE(connection_->CreateView(BuildViewId(2, 1)));
  EXPECT_TRUE(connection_->changes().empty());
}

// Verifies hierarchy changes.
TEST_F(ViewManagerConnectionTest, AddRemoveNotify) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 3), 1));
    EXPECT_TRUE(connection_->changes().empty());

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Remove 3 from its parent.
  {
    ASSERT_TRUE(connection_->RemoveNodeFromParent(BuildNodeId(1, 3), 2));
    EXPECT_TRUE(connection_->changes().empty());

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[0]);
  }
}

// Verifies AddNode fails when node is already in position.
TEST_F(ViewManagerConnectionTest, AddNodeWithNoChange) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 3), 1));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Try again, this should fail.
  {
    EXPECT_FALSE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 3), 2));
  }
}

// Verifies AddNode fails when node is already in position.
TEST_F(ViewManagerConnectionTest, AddAncestorFails) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 3), 1));
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Try to make 2 a child of 3, this should fail since 2 is an ancestor of 3.
  {
    EXPECT_FALSE(connection_->AddNode(BuildNodeId(1, 3), BuildNodeId(1, 2), 2));
  }
}

// Verifies adding with an invalid id fails.
TEST_F(ViewManagerConnectionTest, AddWithInvalidServerId) {
  // Create two nodes.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  // Make 2 a child of 1. Supply an invalid change id, which should fail.
  ASSERT_FALSE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 0));
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, AddToRoot) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 21)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 21.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 21), BuildNodeId(1, 3), 1));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Make 21 a child of 1.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 21), 2));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,21 new_parent=1,1 old_parent=null",
        changes[0]);
  }
}

// Verifies HierarchyChanged is correctly sent for various adds/removes.
TEST_F(ViewManagerConnectionTest, NodeHierarchyChangedNodes) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 11)));
  // Make 11 a child of 2.
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 11), 1));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 2));

    // Client 2 should get a hierarchy change that includes the new nodes as it
    // has not yet seen them.
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,2 new_parent=1,1 old_parent=null",
        changes[0]);
    EXPECT_EQ("[node=1,2 parent=1,1 view=null],"
              "[node=1,11 parent=1,2 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }

  // Add 1 to the root.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 3));

    // Client 2 should get a hierarchy change that includes the new nodes as it
    // has not yet seen them.
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,1 new_parent=null old_parent=null",
        changes[0]);
    EXPECT_EQ(std::string(), ChangeNodeDescription(connection2_->changes()));
  }

  // Remove 1 from its parent.
  {
    ASSERT_TRUE(connection_->RemoveNodeFromParent(BuildNodeId(1, 1), 4));
    EXPECT_TRUE(connection_->changes().empty());

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=4 node=1,1 new_parent=null old_parent=null",
        changes[0]);
  }

  // Create another node, 111, parent it to 11.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 111)));
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 11), BuildNodeId(1, 111),
                                     5));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=5 node=1,111 new_parent=1,11 "
        "old_parent=null", changes[0]);
    EXPECT_EQ("[node=1,111 parent=1,11 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }

  // Reattach 1 to the root.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 6));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=6 node=1,1 new_parent=null old_parent=null",
        changes[0]);
    EXPECT_EQ(std::string(), ChangeNodeDescription(connection2_->changes()));
  }
}

TEST_F(ViewManagerConnectionTest, NodeHierarchyChangedAddingKnownToUnknown) {
  // Create the following structure: root -> 1 -> 11 and 2->21 (2 has no
  // parent).
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 11)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 21)));

  // Set up the hierarchy.
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 11), 2));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 21), 3));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  {
    EXPECT_EQ("[node=1,1 parent=null view=null],"
              "[node=1,11 parent=1,1 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }

  // Remove 11, should result in a delete (since 11 is no longer in connection
  // 2's root).
  {
    ASSERT_TRUE(connection_->RemoveNodeFromParent(BuildNodeId(1, 11), 4));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=4 node=1,11", changes[0]);
  }

  // Add 2 to 1.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 5));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=5 node=1,2 new_parent=1,1 old_parent=null",
        changes[0]);
    EXPECT_EQ("[node=1,2 parent=1,1 view=null],"
              "[node=1,21 parent=1,2 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }
}

// Verifies DeleteNode works.
TEST_F(ViewManagerConnectionTest, DeleteNode) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 1));
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("HierarchyChanged change_id=1 node=1,2 new_parent=1,1 "
              "old_parent=null", changes[0]);
  }

  // Delete 2.
  {
    ASSERT_TRUE(connection_->DeleteNode(BuildNodeId(1, 2)));
    EXPECT_TRUE(connection_->changes().empty());

    // TODO(sky): fix this, client should not get ServerChangeIdAdvanced.
    connection2_->DoRunLoopUntilChangesCount(2);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=2 node=1,2", changes[0]);
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[1]);
  }
}

// Verifies DeleteNode isn't allowed from a separate connection.
TEST_F(ViewManagerConnectionTest, DeleteNodeFromAnotherConnectionDisallowed) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_FALSE(connection2_->DeleteNode(BuildNodeId(1, 1)));
}

// Verifies DeleteView isn't allowed from a separate connection.
TEST_F(ViewManagerConnectionTest, DeleteViewFromAnotherConnectionDisallowed) {
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 1)));
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_FALSE(connection2_->DeleteView(BuildViewId(1, 1)));
}

// Verifies if a node was deleted and then reused that other clients are
// properly notified.
TEST_F(ViewManagerConnectionTest, ReuseDeletedNodeId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  // Add 2 to 1.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 1));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    EXPECT_EQ(
        "HierarchyChanged change_id=1 node=1,2 new_parent=1,1 old_parent=null",
        changes[0]);
    EXPECT_EQ("[node=1,2 parent=1,1 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }

  // Delete 2.
  {
    ASSERT_TRUE(connection_->DeleteNode(BuildNodeId(1, 2)));

    // TODO(sky): fix this, shouldn't get ServerChangeIdAdvanced.
    connection2_->DoRunLoopUntilChangesCount(2);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=2 node=1,2", changes[0]);
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[1]);
  }

  // Create 2 again, and add it back to 1. Should get the same notification.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 3));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,2 new_parent=1,1 old_parent=null",
        changes[0]);
    EXPECT_EQ("[node=1,2 parent=1,1 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }
}

// Assertions around setting a view.
TEST_F(ViewManagerConnectionTest, SetView) {
  // Create nodes 1, 2 and 3 and the view 11. Nodes 2 and 3 are parented to 1.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 11)));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 1));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 3), 2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Set view 11 on node 1.
  {
    ASSERT_TRUE(connection_->SetView(BuildNodeId(1, 1),
                                     BuildViewId(1, 11)));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=1,11 old_view=null",
              changes[0]);
  }

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(connection_->SetView(BuildNodeId(1, 2), BuildViewId(1, 11)));

    connection2_->DoRunLoopUntilChangesCount(2);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=null old_view=1,11",
              changes[0]);
    EXPECT_EQ("ViewReplaced node=1,2 new_view=1,11 old_view=null",
              changes[1]);
  }
}

// Verifies deleting a node with a view sends correct notifications.
TEST_F(ViewManagerConnectionTest, DeleteNodeWithView) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 11)));

  // Set view 11 on node 2.
  ASSERT_TRUE(connection_->SetView(BuildNodeId(1, 2), BuildViewId(1, 11)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Delete node 2. The second connection should not see this because the node
  // was not known to it.
  {
    ASSERT_TRUE(connection_->DeleteNode(BuildNodeId(1, 2)));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Parent 3 to 1.
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 3), 2));
  connection2_->DoRunLoopUntilChangesCount(1);

  // Set view 11 on node 3.
  {
    ASSERT_TRUE(connection_->SetView(BuildNodeId(1, 3), BuildViewId(1, 11)));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,3 new_view=1,11 old_view=null", changes[0]);
  }

  // Delete 3.
  {
    ASSERT_TRUE(connection_->DeleteNode(BuildNodeId(1, 3)));

    // TODO(sky): shouldn't get ServerChangeIdAdvanced here.
    connection2_->DoRunLoopUntilChangesCount(2);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=3 node=1,3", changes[0]);
    EXPECT_EQ("ServerChangeIdAdvanced 4", changes[1]);
  }
}

// Sets view from one connection on another.
TEST_F(ViewManagerConnectionTest, SetViewFromSecondConnection) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  // Create a view in the second connection.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 51)));

  // Attach view to node 1 in the first connection.
  {
    ASSERT_TRUE(connection2_->SetView(BuildNodeId(1, 1), BuildViewId(2, 51)));
    connection_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=2,51 old_view=null", changes[0]);
  }

  // Shutdown the second connection and verify view is removed.
  {
    DestroySecondConnection();
    connection_->DoRunLoopUntilChangesCount(2);
    const Changes changes(ChangesToDescription1(connection_->changes()));
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=null old_view=2,51", changes[0]);
    EXPECT_EQ("ViewDeleted view=2,51", changes[1]);
  }
}

// Assertions for GetNodeTree.
TEST_F(ViewManagerConnectionTest, GetNodeTree) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Create 11 in first connection and make it a child of 1.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 11)));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 11), 2));

  // Create two nodes in second connection, 2 and 3, both children of 1.
  ASSERT_TRUE(connection2_->CreateNode(BuildNodeId(2, 2)));
  ASSERT_TRUE(connection2_->CreateNode(BuildNodeId(2, 3)));
  ASSERT_TRUE(connection2_->AddNode(BuildNodeId(1, 1), BuildNodeId(2, 2), 3));
  ASSERT_TRUE(connection2_->AddNode(BuildNodeId(1, 1), BuildNodeId(2, 3), 4));

  // Attach view to node 11 in the first connection.
  ASSERT_TRUE(connection_->CreateView(BuildViewId(1, 51)));
  ASSERT_TRUE(connection_->SetView(BuildNodeId(1, 11), BuildViewId(1, 51)));

  // Verifies GetNodeTree() on the root.
  {
    std::vector<TestNode> nodes;
    connection_->GetNodeTree(BuildNodeId(0, 1), &nodes);
    ASSERT_EQ(5u, nodes.size());
    EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[1].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=1,51", nodes[2].ToString());
    EXPECT_EQ("node=2,2 parent=1,1 view=null", nodes[3].ToString());
    EXPECT_EQ("node=2,3 parent=1,1 view=null", nodes[4].ToString());
  }

  // Verifies GetNodeTree() on the node 1,1.
  {
    std::vector<TestNode> nodes;
    connection2_->GetNodeTree(BuildNodeId(1, 1), &nodes);
    ASSERT_EQ(4u, nodes.size());
    EXPECT_EQ("node=1,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=1,51", nodes[1].ToString());
    EXPECT_EQ("node=2,2 parent=1,1 view=null", nodes[2].ToString());
    EXPECT_EQ("node=2,3 parent=1,1 view=null", nodes[3].ToString());
  }

  // Connection 2 shouldn't be able to get the root tree.
  {
    std::vector<TestNode> nodes;
    connection2_->GetNodeTree(BuildNodeId(0, 1), &nodes);
    ASSERT_EQ(0u, nodes.size());
  }
}

TEST_F(ViewManagerConnectionTest, SetNodeBounds) {
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  ASSERT_TRUE(connection_->SetNodeBounds(BuildNodeId(1, 1),
                                         gfx::Rect(0, 0, 100, 100)));

  connection2_->DoRunLoopUntilChangesCount(1);
  const Changes changes(ChangesToDescription1(connection2_->changes()));
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ("BoundsChanged node=1,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100",
            changes[0]);

  // Should not be possible to change the bounds of a node created by another
  // connection.
  ASSERT_FALSE(connection2_->SetNodeBounds(BuildNodeId(1, 1),
                                           gfx::Rect(0, 0, 0, 0)));
}

// Various assertions around SetRoots.
TEST_F(ViewManagerConnectionTest, SetRoots) {
  // Create 1, 2, and 3 in the first connection.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 3)));

  // Parent 1 to the root.
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));

  // Establish the second connection and give it the roots 1 and 3.
  {
    ASSERT_NO_FATAL_FAILURE(EstablishSecondConnectionWithRoots(
                                BuildNodeId(1, 1), BuildNodeId(1, 3)));
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnConnectionEstablished", changes[0]);
    EXPECT_EQ("[node=1,1 parent=null view=null],"
              "[node=1,3 parent=null view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }

  // Create 4 and add it to the root, connection 2 should only get id advanced.
  {
    ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 4)));
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 4), 2));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[0]);
  }

  // Move 4 under 3, this should expose 4 to the client.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 3), BuildNodeId(1, 4), 3));
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,4 new_parent=1,3 "
        "old_parent=null", changes[0]);
    EXPECT_EQ("[node=1,4 parent=1,3 view=null]",
              ChangeNodeDescription(connection2_->changes()));
  }

  // Move 4 under 2, since 2 isn't a root client should get a delete.
  {
    ASSERT_TRUE(connection_->AddNode(BuildNodeId(1, 2), BuildNodeId(1, 4), 4));
    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=4 node=1,4", changes[0]);
  }

  // Delete 4, client shouldn't receive a delete since it should no longer know
  // about 4.
  {
    ASSERT_TRUE(connection_->DeleteNode(BuildNodeId(1, 4)));

    connection2_->DoRunLoopUntilChangesCount(1);
    const Changes changes(ChangesToDescription1(connection2_->changes()));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 6", changes[0]);
  }
}

// Verify AddNode fails when trying to manipulate nodes in other roots.
TEST_F(ViewManagerConnectionTest, CantMoveNodesFromOtherRoot) {
  // Create 1 and 2 in the first connection.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Try to move 2 to be a child of 1 from connection 2. This should fail as 2
  // should not be able to access 1.
  ASSERT_FALSE(connection2_->AddNode(BuildNodeId(1, 1), BuildNodeId(1, 2), 1));

  // Try to reparent 1 to the root. A connection is not allowed to reparent its
  // roots.
  ASSERT_FALSE(connection2_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));
}

// Verify RemoveNodeFromParent fails for nodes that are descendants of the
// roots.
TEST_F(ViewManagerConnectionTest, CantRemoveNodesInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 2), 2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Connection 2 should not be able to remove node 2 or 1 from its parent.
  ASSERT_FALSE(connection2_->RemoveNodeFromParent(BuildNodeId(1, 2), 3));
  ASSERT_FALSE(connection2_->RemoveNodeFromParent(BuildNodeId(1, 1), 3));

  // Create nodes 10 and 11 in 2.
  ASSERT_TRUE(connection2_->CreateNode(BuildNodeId(2, 10)));
  ASSERT_TRUE(connection2_->CreateNode(BuildNodeId(2, 11)));

  // Parent 11 to 10.
  ASSERT_TRUE(connection2_->AddNode(BuildNodeId(2, 10), BuildNodeId(2, 11), 3));
  // Remove 11 from 10.
  ASSERT_TRUE(connection2_->RemoveNodeFromParent( BuildNodeId(2, 11), 4));

  // Verify nothing was actually removed.
  {
    std::vector<TestNode> nodes;
    connection_->GetNodeTree(BuildNodeId(0, 1), &nodes);
    ASSERT_EQ(3u, nodes.size());
    EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[1].ToString());
    EXPECT_EQ("node=1,2 parent=0,1 view=null", nodes[2].ToString());
  }
}

// Verify SetView fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, CantRemoveSetViewInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 2), 2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Create a view in the second connection.
  ASSERT_TRUE(connection2_->CreateView(BuildViewId(2, 51)));

  // Connection 2 should be able to set the view on node 1 (it's root), but not
  // on 2.
  ASSERT_TRUE(connection2_->SetView(BuildNodeId(1, 1), BuildViewId(2, 51)));
  ASSERT_FALSE(connection2_->SetView(BuildNodeId(1, 2), BuildViewId(2, 51)));
}

// Verify GetNodeTree fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, CantGetNodeTreeOfOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 1)));
  ASSERT_TRUE(connection_->CreateNode(BuildNodeId(1, 2)));

  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 1), 1));
  ASSERT_TRUE(connection_->AddNode(BuildNodeId(0, 1), BuildNodeId(1, 2), 2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  std::vector<TestNode> nodes;

  // Should get nothing for the root.
  connection2_->GetNodeTree(BuildNodeId(0, 1), &nodes);
  ASSERT_TRUE(nodes.empty());

  // Should get nothing for node 2.
  connection2_->GetNodeTree(BuildNodeId(1, 2), &nodes);
  ASSERT_TRUE(nodes.empty());

  // Should get node 1 if asked for.
  connection2_->GetNodeTree(BuildNodeId(1, 1), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ("node=1,1 parent=null view=null", nodes[0].ToString());
}

// TODO(sky): add coverage of test that destroys connections and ensures other
// connections get deletion notification (or advanced server id).

// TODO(sky): need to better track changes to initial connection. For example,
// that SetBounsdNodes/AddNode and the like don't result in messages to the
// originating connection.

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
