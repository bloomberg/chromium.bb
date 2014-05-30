// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/application/connect.h"
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

// BackgroundConnection is used when a child ViewManagerConnection is created by
// way of Connect(). BackgroundConnection is created on a background thread (the
// thread where the shell lives). All public methods are to be invoked on the
// main thread. They wait for a result (by spinning a nested message loop),
// calling through to background thread, then the underlying IViewManager* and
// respond back to the calling thread to return control to the test.
class BackgroundConnection : public TestChangeTracker::Delegate {
 public:
  BackgroundConnection(TestChangeTracker* tracker,
                       base::MessageLoop* loop)
      : tracker_(tracker),
        main_loop_(loop),
        background_loop_(base::MessageLoop::current()),
        view_manager_(NULL),
        quit_count_(0) {
    main_loop_->PostTask(FROM_HERE,
                         base::Bind(&BackgroundConnection::SetInstance, this));
  }

  virtual ~BackgroundConnection() {
    instance_ = NULL;
  }

  void set_view_manager(IViewManager* view_manager) {
    view_manager_ = view_manager;
  }

  // Runs a message loop until the single instance has been created.
  static BackgroundConnection* WaitForInstance() {
    if (!instance_)
      RunMainLoop();
    return instance_;
  }

  // Runs the main loop until |count| changes have been received.
  std::vector<Change> DoRunLoopUntilChangesCount(size_t count) {
    background_loop_->PostTask(FROM_HERE,
                               base::Bind(&BackgroundConnection::SetQuitCount,
                                          base::Unretained(this), count));
    // Run the current message loop. When the quit count is reached we'll quit.
    RunMainLoop();
    return changes_;
  }

  // The following functions mirror that of IViewManager. They bounce the
  // function to the right thread and return the result.
  bool CreateNode(TransportNodeId node_id) {
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundConnection::CreateNodeOnBackgroundThread,
                   base::Unretained(this), node_id, &result));
    RunMainLoop();
    return result;
  }
  bool AddNode(TransportNodeId parent,
               TransportNodeId child,
               TransportChangeId server_change_id) {
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundConnection::AddNodeOnBackgroundThread,
                   base::Unretained(this), parent, child, server_change_id,
                   &result));
    RunMainLoop();
    return result;
  }
  bool RemoveNodeFromParent(TransportNodeId node_id,
                            TransportChangeId server_change_id) {
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &BackgroundConnection::RemoveNodeFromParentOnBackgroundThread,
            base::Unretained(this), node_id, server_change_id, &result));
    RunMainLoop();
    return result;
  }
  bool SetView(TransportNodeId node_id, TransportViewId view_id) {
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &BackgroundConnection::SetViewOnBackgroundThread,
            base::Unretained(this), node_id, view_id, &result));
    RunMainLoop();
    return result;
  }
  bool CreateView(TransportViewId view_id) {
    bool result = false;
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &BackgroundConnection::CreateViewOnBackgroundThread,
            base::Unretained(this), view_id, &result));
    RunMainLoop();
    return result;
  }
  void GetNodeTree(TransportNodeId node_id, std::vector<TestNode>* nodes) {
    background_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &BackgroundConnection::GetNodeTreeOnBackgroundThread,
            base::Unretained(this), node_id, nodes));
    RunMainLoop();
  }

 private:
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
        base::Bind(&BackgroundConnection::QuitCountReachedOnMain,
                   base::Unretained(this), changes));
  }

  void QuitCountReachedOnMain(const std::vector<Change>& changes) {
    changes_ = changes;
    DCHECK(main_run_loop_);
    main_run_loop_->Quit();
  }

  static void SetInstance(BackgroundConnection* instance) {
    DCHECK(!instance_);
    instance_ = instance;
    if (main_run_loop_)
      main_run_loop_->Quit();
  }

  // Callbacks from the various IViewManager functions.
  void GotResultOnBackgroundThread(bool* result_cache, bool result) {
    *result_cache = result;
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundConnection::QuitOnMainThread,
                   base::Unretained(this)));
  }

  void GotNodeTreeOnBackgroundThread(std::vector<TestNode>* nodes,
                                     Array<INodePtr> results) {
    INodesToTestNodes(results, nodes);
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundConnection::QuitOnMainThread,
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
        base::Bind(&BackgroundConnection::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void AddNodeOnBackgroundThread(TransportNodeId parent,
                                 TransportNodeId child,
                                 TransportChangeId server_change_id,
                                 bool* result) {
    view_manager_->AddNode(
        parent, child, server_change_id,
        base::Bind(&BackgroundConnection::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void RemoveNodeFromParentOnBackgroundThread(
      TransportNodeId node_id,
      TransportChangeId server_change_id,
      bool* result) {
    view_manager_->RemoveNodeFromParent(node_id, server_change_id,
        base::Bind(&BackgroundConnection::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void SetViewOnBackgroundThread(TransportNodeId node_id,
                                 TransportViewId view_id,
                                 bool* result) {
    view_manager_->SetView(node_id, view_id,
        base::Bind(&BackgroundConnection::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void CreateViewOnBackgroundThread(TransportViewId view_id, bool* result) {
    view_manager_->CreateView(view_id,
        base::Bind(&BackgroundConnection::GotResultOnBackgroundThread,
                   base::Unretained(this), result));
  }
  void GetNodeTreeOnBackgroundThread(TransportNodeId node_id,
                                     std::vector<TestNode>* nodes) {
    view_manager_->GetNodeTree(node_id,
        base::Bind(&BackgroundConnection::GotNodeTreeOnBackgroundThread,
                   base::Unretained(this), nodes));
  }

  // TestChangeTracker::Delegate:
  virtual void OnChangeAdded() OVERRIDE {
    if (quit_count_ > 0 && --quit_count_ == 0)
      QuitCountReached();
  }

  static BackgroundConnection* instance_;
  static base::RunLoop* main_run_loop_;

  TestChangeTracker* tracker_;

  // MessageLoop of the test.
  base::MessageLoop* main_loop_;

  // MessageLoop BackgroundConnection lives on.
  base::MessageLoop* background_loop_;

  IViewManager* view_manager_;

  // Number of changes we're waiting on until we quit the current loop.
  size_t quit_count_;

  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundConnection);
};

// static
BackgroundConnection* BackgroundConnection::instance_ = NULL;

// static
base::RunLoop* BackgroundConnection::main_run_loop_ = NULL;

class TestViewManagerClientConnection
    : public InterfaceImpl<IViewManagerClient> {
 public:
  explicit TestViewManagerClientConnection(base::MessageLoop* loop)
      : connection_(&tracker_, loop) {
    tracker_.set_delegate(&connection_);
  }

  // InterfaceImp:
  virtual void OnConnectionEstablished() OVERRIDE {
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
  BackgroundConnection connection_;

  DISALLOW_COPY_AND_ASSIGN(TestViewManagerClientConnection);
};

// Used with IViewManager::Connect(). Creates a TestViewManagerClientConnection,
// which creates and owns the BackgroundConnection.
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

// Callback that results in a vector of INodes. The INodes are converted to
// TestNodes.
void INodesCallback(std::vector<TestNode>* test_nodes,
                    Array<INodePtr> data) {
  INodesToTestNodes(data, test_nodes);
  current_run_loop->Quit();
}

// Creates an id used for transport from the specified parameters.
TransportNodeId CreateNodeId(TransportConnectionId connection_id,
                             TransportConnectionSpecificNodeId node_id) {
  return (connection_id << 16) | node_id;
}

// Creates an id used for transport from the specified parameters.
TransportViewId CreateViewId(TransportConnectionId connection_id,
                             TransportConnectionSpecificViewId view_id) {
  return (connection_id << 16) | view_id;
}

// Creates a node with the specified id. Returns true on success. Blocks until
// we get back result from server.
bool CreateNode(IViewManager* view_manager,
                TransportConnectionId connection_id,
                TransportConnectionSpecificNodeId node_id) {
  bool result = false;
  view_manager->CreateNode(CreateNodeId(connection_id, node_id),
                           base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Deletes a view, blocking until done.
bool DeleteNode(IViewManager* view_manager, TransportNodeId node_id) {
  bool result = false;
  view_manager->DeleteNode(node_id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Deletes a node, blocking until done.
bool DeleteView(IViewManager* view_manager, TransportViewId view_id) {
  bool result = false;
  view_manager->DeleteView(view_id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

bool SetNodeBounds(IViewManager* view_manager,
                   TransportNodeId node_id,
                   const gfx::Rect& bounds) {
  bool result = false;
  view_manager->SetNodeBounds(node_id, Rect::From(bounds),
                              base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Adds a node, blocking until done.
bool AddNode(IViewManager* view_manager,
             TransportNodeId parent,
             TransportNodeId child,
             TransportChangeId server_change_id) {
  bool result = false;
  view_manager->AddNode(parent, child, server_change_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Removes a node, blocking until done.
bool RemoveNodeFromParent(IViewManager* view_manager,
                          TransportNodeId node_id,
                          TransportChangeId server_change_id) {
  bool result = false;
  view_manager->RemoveNodeFromParent(
      node_id, server_change_id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

void GetNodeTree(IViewManager* view_manager,
                 TransportNodeId node_id,
                 std::vector<TestNode>* nodes) {
  view_manager->GetNodeTree(node_id, base::Bind(&INodesCallback, nodes));
  DoRunLoop();
}

// Creates a view with the specified id. Returns true on success. Blocks until
// we get back result from server.
bool CreateView(IViewManager* view_manager,
                TransportConnectionId connection_id,
                TransportConnectionSpecificViewId id) {
  bool result = false;
  view_manager->CreateView(CreateViewId(connection_id, id),
                           base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Sets a view on the specified node. Returns true on success. Blocks until we
// get back result from server.
bool SetView(IViewManager* view_manager,
             TransportNodeId node_id,
             TransportViewId view_id) {
  bool result = false;
  view_manager->SetView(node_id, view_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

bool Connect(IViewManager* view_manager,
             const std::string& url,
             TransportNodeId id,
             TransportNodeId id2) {
  bool result = false;
  std::vector<TransportNodeId> node_ids;
  node_ids.push_back(id);
  if (id2 != 0)
    node_ids.push_back(id2);
  view_manager->Connect(url, Array<uint32_t>::From(node_ids),
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

}  // namespace

typedef std::vector<std::string> Changes;

class MainLoopTrackerDelegate : public TestChangeTracker::Delegate {
 public:
  explicit MainLoopTrackerDelegate(TestChangeTracker* tracker)
      : tracker_(tracker),
        quit_count_(0) {}

  void DoRunLoopUntilChangesCount(size_t count) {
    if (tracker_->changes()->size() >= count)
      return;
    quit_count_ = count - tracker_->changes()->size();
    DoRunLoop();
  }

  // TestChangeTracker::Delegate:
  virtual void OnChangeAdded() OVERRIDE {
    if (quit_count_ > 0 && --quit_count_ == 0)
      current_run_loop->Quit();
  }

 private:
  TestChangeTracker* tracker_;
  size_t quit_count_;

  DISALLOW_COPY_AND_ASSIGN(MainLoopTrackerDelegate);
};

class ViewManagerClientImpl : public IViewManagerClient {
 public:
  ViewManagerClientImpl()
      : id_(0),
        next_server_change_id_(0),
        main_loop_tracker_delegate_(&tracker_) {
    tracker_.set_delegate(&main_loop_tracker_delegate_);
  }

  TransportConnectionId id() const { return id_; }

  TransportChangeId next_server_change_id() const {
    return next_server_change_id_;
  }
  const std::vector<TestNode>& initial_nodes() const {
    return initial_nodes_;
  }
  const std::vector<TestNode>& hierarchy_changed_nodes() const {
    return hierarchy_changed_nodes_;
  }

  Changes GetAndClearChanges() {
    Changes changes = ChangesToDescription1(*tracker_.changes());
    tracker_.changes()->clear();
    return changes;
  }

  void ClearId() {
    id_ = 0;
  }

  void WaitForId() {
    DCHECK_EQ(0, id_);
    DoRunLoopUntilChangesCount(1);
  }

  void DoRunLoopUntilChangesCount(size_t count) {
    main_loop_tracker_delegate_.DoRunLoopUntilChangesCount(count);
  }

 private:
  // IViewManagerClient overrides:
  virtual void OnViewManagerConnectionEstablished(
      TransportConnectionId connection_id,
      TransportChangeId next_server_change_id,
      mojo::Array<INodePtr> nodes) OVERRIDE {
    id_ = connection_id;
    next_server_change_id_ = next_server_change_id;
    initial_nodes_.clear();
    INodesToTestNodes(nodes, &initial_nodes_);
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
      mojo::Array<INodePtr> nodes) OVERRIDE {
    hierarchy_changed_nodes_.clear();
    INodesToTestNodes(nodes, &hierarchy_changed_nodes_);
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

  TransportConnectionId id_;
  TransportChangeId next_server_change_id_;

  // Set of nodes sent when connection created.
  std::vector<TestNode> initial_nodes_;

  // Nodes sent from last OnNodeHierarchyChanged.
  std::vector<TestNode> hierarchy_changed_nodes_;

  TestChangeTracker tracker_;
  MainLoopTrackerDelegate main_loop_tracker_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

class ViewManagerConnectionTest : public testing::Test {
 public:
  ViewManagerConnectionTest() : background_connection_(NULL) {}

  virtual void SetUp() OVERRIDE {
    test_helper_.Init();

    ConnectToService(test_helper_.service_provider(),
                     "mojo:mojo_view_manager",
                     &view_manager_);
    view_manager_.set_client(&client_);

    client_.WaitForId();
    client_.GetAndClearChanges();

    test_helper_.SetLoaderForURL(
        scoped_ptr<ServiceLoader>(new ConnectServiceLoader()),
        GURL(kTestServiceURL));
  }

 protected:
  // Creates a second connection to the viewmanager.
  void EstablishSecondConnection() {
    ConnectToService(test_helper_.service_provider(),
                     "mojo:mojo_view_manager",
                     &view_manager2_);
    view_manager2_.set_client(&client2_);

    client2_.WaitForId();
    client2_.GetAndClearChanges();
  }

  std::vector<Change> EstablishBackgroundConnectionWithRoots(
      TransportNodeId id1,
      TransportNodeId id2) {
    Connect(view_manager_.get(), kTestServiceURL, id1, id2);
    background_connection_ = BackgroundConnection::WaitForInstance();
    return background_connection_->DoRunLoopUntilChangesCount(1);
  }

  void EstablishBackgroundConnectionWithRoot1() {
    std::vector<Change> changes(
        EstablishBackgroundConnectionWithRoots(CreateNodeId(1, 1), 0));
    if (HasFatalFailure())
      return;
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnConnectionEstablished", ChangesToDescription1(changes)[0]);
    EXPECT_EQ("[node=1,1 parent=null view=null]",
              ChangeNodeDescription(changes));
  }

  void DestroySecondConnection() {
    view_manager2_.reset();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;

  ViewManagerClientImpl client_;
  IViewManagerPtr view_manager_;

  ViewManagerClientImpl client2_;
  IViewManagerPtr view_manager2_;

  BackgroundConnection* background_connection_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnectionTest);
};

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, ValidId) {
  // All these tests assume 1 for the client id. The only real assertion here is
  // the client id is not zero, but adding this as rest of code here assumes 1.
  EXPECT_EQ(1, client_.id());

  // Change ids start at 1 as well.
  EXPECT_EQ(static_cast<TransportChangeId>(1), client_.next_server_change_id());
}

// Verifies two clients/connections get different ids.
TEST_F(ViewManagerConnectionTest, TwoClientsGetDifferentConnectionIds) {
  EstablishSecondConnection();
  EXPECT_NE(0, client2_.id());
  EXPECT_NE(client_.id(), client2_.id());
}

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, CreateNode) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));

  // Can't create a node with the same id.
  ASSERT_FALSE(CreateNode(view_manager_.get(), 1, 1));
}

TEST_F(ViewManagerConnectionTest, CreateNodeFailsWithBogusConnectionId) {
  EXPECT_FALSE(CreateNode(view_manager_.get(), 2, 1));
}

TEST_F(ViewManagerConnectionTest, CreateViewFailsWithBogusConnectionId) {
  EXPECT_FALSE(CreateView(view_manager_.get(), 2, 1));
}

// Verifies hierarchy changes.
TEST_F(ViewManagerConnectionTest, AddRemoveNotify) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Remove 2 from its parent.
  {
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 2),
                                     2));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[0]);
  }
}

// Verifies AddNode fails when node is already in position.
TEST_F(ViewManagerConnectionTest, AddNodeWithNoChange) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Try again, this should fail.
  {
    EXPECT_FALSE(AddNode(view_manager_.get(),
                         CreateNodeId(client_.id(), 1),
                         CreateNodeId(client_.id(), 2),
                         2));
    Changes changes(client_.GetAndClearChanges());
    EXPECT_TRUE(changes.empty());
  }
}

// Verifies AddNode fails when node is already in position.
TEST_F(ViewManagerConnectionTest, AddAncestorFails) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Try to make 1 a child of 2, this should fail since 1 is an ancestor of 2.
  {
    EXPECT_FALSE(AddNode(view_manager_.get(),
                         CreateNodeId(client_.id(), 2),
                         CreateNodeId(client_.id(), 1),
                         2));
    Changes changes(client_.GetAndClearChanges());
    EXPECT_TRUE(changes.empty());
  }
}

// Verifies adding with an invalid id fails.
TEST_F(ViewManagerConnectionTest, AddWithInvalidServerId) {
  // Create two nodes.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  // Make 2 a child of 1. Supply an invalid change id, which should fail.
  {
    ASSERT_FALSE(AddNode(view_manager_.get(),
                         CreateNodeId(client_.id(), 1),
                         CreateNodeId(client_.id(), 2),
                         0));
    Changes changes(client_.GetAndClearChanges());
    EXPECT_TRUE(changes.empty());
  }
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, AddToRoot) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 21));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 3));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 3 a child of 21.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 3),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Make 21 a child of the root.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 21),
                        2));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,21 new_parent=0,1 old_parent=null",
        changes[0]);
  }
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, NodeHierarchyChangedNodes) {
  // Create nodes 1 and 11 with 1 parented to the root and 11 a child of 1.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 11));

  // Make 11 a child of 1.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 11),
                        1));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());
  }

  EstablishSecondConnection();

  // Make 1 a child of the root.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        2));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    // Client 2 should get a hierarchy change that includes the new nodes as it
    // has not yet seen them.
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=null", nodes[1].ToString());
  }

  // Remove 1 from the root.
  {
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 1),
                                     3));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,1 new_parent=null old_parent=0,1",
        changes[0]);
  }

  // Create another node, 111, parent it to 11.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 111));
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 11),
                        CreateNodeId(client_.id(), 111),
                        4));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    // Even though 11 isn't attached to the root client 2 is still notified of
    // the change because it was told about 11.
    EXPECT_EQ(
        "HierarchyChanged change_id=4 node=1,111 new_parent=1,11 "
        "old_parent=null", changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,111 parent=1,11 view=null", nodes[0].ToString());
  }

  // Reattach 1 to the root.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        5));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=5 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    ASSERT_TRUE(client2_.hierarchy_changed_nodes().empty());
  }
}

TEST_F(ViewManagerConnectionTest, NodeHierarchyChangedAddingKnownToUnknown) {
  // Create the following structure: root -> 1 -> 11 and 2->21 (2 has no
  // parent).
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 11));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 21));

  // Set up the hierarchy.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        1));
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 11),
                        2));
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateNodeId(client_.id(), 21),
                        3));
  }

  EstablishSecondConnection();

  // Remove 11.
  {
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 11),
                                     4));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=4 node=1,11 new_parent=null old_parent=1,1",
        changes[0]);
    EXPECT_TRUE(client2_.hierarchy_changed_nodes().empty());
  }

  // Add 11 to 21. As client2 knows about 11 it should receive the new
  // hierarchy.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 11),
                        5));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=5 node=1,11 new_parent=1,21 "
        "old_parent=null", changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,2 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,21 parent=1,2 view=null", nodes[1].ToString());
  }
}

// Verifies connection on told descendants of the root when connecting.
TEST_F(ViewManagerConnectionTest, GetInitialNodesOnInit) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 21));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 3));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 3 a child of 21.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 3),
                        1));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());
  }

  // Make 21 a child of the root.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 21),
                        2));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());
  }

  EstablishSecondConnection();
  // Should get notification of children of the root.
  const std::vector<TestNode>& nodes(client2_.initial_nodes());
  ASSERT_EQ(3u, nodes.size());
  EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
  EXPECT_EQ("node=1,21 parent=0,1 view=null", nodes[1].ToString());
  EXPECT_EQ("node=1,3 parent=1,21 view=null", nodes[2].ToString());
}

// Verifies DeleteNode works.
TEST_F(ViewManagerConnectionTest, DeleteNode) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Add 1 to the root
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        2));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
  }

  // Delete 1.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 1)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=3 node=1,1", changes[0]);
  }
}

// Verifies DeleteNode isn't allowed from a separate connection.
TEST_F(ViewManagerConnectionTest, DeleteNodeFromAnotherConnectionDisallowed) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  EstablishSecondConnection();
  EXPECT_FALSE(DeleteNode(view_manager2_.get(), CreateNodeId(client_.id(), 1)));
}

// Verifies DeleteView isn't allowed from a separate connection.
TEST_F(ViewManagerConnectionTest, DeleteViewFromAnotherConnectionDisallowed) {
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 1));
  EstablishSecondConnection();
  EXPECT_FALSE(DeleteView(view_manager2_.get(), CreateViewId(client_.id(), 1)));
}

// Verifies if a node was deleted and then reused that other clients are
// properly notified.
TEST_F(ViewManagerConnectionTest, ReusedDeletedId) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();

  // Make 1 a child of the root.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        1));
    EXPECT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    EXPECT_EQ(
        "HierarchyChanged change_id=1 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
  }

  // Delete 1.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 1)));
    EXPECT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=2 node=1,1", changes[0]);
  }

  // Create 1 again, and add it back to the root. Should get the same
  // notification.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        3));
    EXPECT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
  }
}

// Assertions around setting a view.
TEST_F(ViewManagerConnectionTest, SetView) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 11));
  ASSERT_TRUE(AddNode(view_manager_.get(), 1, CreateNodeId(1, 1), 1));
  ASSERT_TRUE(AddNode(view_manager_.get(), 1, CreateNodeId(1, 2), 2));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Set view 11 on node 1.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateViewId(client_.id(), 11)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "ViewReplaced node=1,1 new_view=1,11 old_view=null",
        changes[0]);
  }

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateViewId(client_.id(), 11)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(2);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=null old_view=1,11",
              changes[0]);
    EXPECT_EQ("ViewReplaced node=1,2 new_view=1,11 old_view=null",
              changes[1]);
  }
}

// Verifies deleting a node with a view sends correct notifications.
TEST_F(ViewManagerConnectionTest, DeleteNodeWithView) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 11));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Set view 11 on node 1.
  ASSERT_TRUE(SetView(view_manager_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateViewId(client_.id(), 11)));
  client_.GetAndClearChanges();

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Delete node 1. The second connection should not see this because the node
  // was not known to it.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 1)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Parent 2 to the root.
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));
  client2_.DoRunLoopUntilChangesCount(1);
  client2_.GetAndClearChanges();

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateViewId(client_.id(), 11)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,2 new_view=1,11 old_view=null", changes[0]);
  }

  // Delete node.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 2)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(2);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,2 new_view=null old_view=1,11", changes[0]);
    EXPECT_EQ("NodeDeleted change_id=3 node=1,2", changes[1]);
  }
}

// Sets view from one connection on another.
TEST_F(ViewManagerConnectionTest, SetViewFromSecondConnection) {
  EstablishSecondConnection();

  // Create two nodes in first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Create a view in the second connection.
  ASSERT_TRUE(CreateView(view_manager2_.get(), 2, 51));

  // Attach view to node 1 in the first connectoin.
  {
    ASSERT_TRUE(SetView(view_manager2_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateViewId(client2_.id(), 51)));
    client_.DoRunLoopUntilChangesCount(1);
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=2,51 old_view=null", changes[0]);
  }

  // Shutdown the second connection and verify view is removed.
  {
    DestroySecondConnection();
    client_.DoRunLoopUntilChangesCount(2);

    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=null old_view=2,51", changes[0]);
    EXPECT_EQ("ViewDeleted view=2,51", changes[1]);
  }
}

// Assertions for GetNodeTree.
TEST_F(ViewManagerConnectionTest, GetNodeTree) {
  EstablishSecondConnection();

  // Create two nodes in first connection, 1 and 11 (11 is a child of 1).
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 11));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateNodeId(client_.id(), 11),
                      2));

  // Create two nodes in second connection, 2 and 3, both children of the root.
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2, 2));
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2, 3));
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client2_.id(), 2),
                      3));
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client2_.id(), 3),
                      4));

  // Attach view to node 11 in the first connection.
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 51));
  ASSERT_TRUE(SetView(view_manager_.get(),
                      CreateNodeId(client_.id(), 11),
                      CreateViewId(client_.id(), 51)));

  // Verifies GetNodeTree() on the root.
  {
    std::vector<TestNode> nodes;
    GetNodeTree(view_manager2_.get(), CreateNodeId(0, 1), &nodes);
    ASSERT_EQ(5u, nodes.size());
    EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[1].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=1,51", nodes[2].ToString());
    EXPECT_EQ("node=2,2 parent=0,1 view=null", nodes[3].ToString());
    EXPECT_EQ("node=2,3 parent=0,1 view=null", nodes[4].ToString());
  }

  // Verifies GetNodeTree() on the node 1,1.
  {
    std::vector<TestNode> nodes;
    GetNodeTree(view_manager2_.get(), CreateNodeId(1, 1), &nodes);
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=1,51", nodes[1].ToString());
  }
}

TEST_F(ViewManagerConnectionTest, SetNodeBounds) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(1, 1),
                      1));
  EstablishSecondConnection();

  ASSERT_TRUE(SetNodeBounds(view_manager_.get(),
                            CreateNodeId(1, 1),
                            gfx::Rect(0, 0, 100, 100)));

  client2_.DoRunLoopUntilChangesCount(1);
  Changes changes(client2_.GetAndClearChanges());
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ("BoundsChanged node=1,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100",
            changes[0]);

  // Should not be possible to change the bounds of a node created by another
  // connection.
  ASSERT_FALSE(SetNodeBounds(view_manager2_.get(),
                             CreateNodeId(1, 1),
                             gfx::Rect(0, 0, 0, 0)));
}

// Various assertions around SetRoots.
TEST_F(ViewManagerConnectionTest, SetRoots) {
  // Create 1, 2, and 3 in the first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 3));

  // Parent 1 to the root.
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));

  // Establish the second connection and give it the roots 1 and 3.
  {
    std::vector<Change> changes(EstablishBackgroundConnectionWithRoots(
                                    CreateNodeId(1, 1), CreateNodeId(1, 3)));
    if (HasFatalFailure())
      return;
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnConnectionEstablished", ChangesToDescription1(changes)[0]);
    EXPECT_EQ("[node=1,1 parent=null view=null],"
              "[node=1,3 parent=null view=null]",
              ChangeNodeDescription(changes));
  }

  // Create 4 and add it to the root, connection 2 should only get id advanced.
  {
    ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 4));
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 4),
                        2));
    Changes changes = ChangesToDescription1(
        background_connection_->DoRunLoopUntilChangesCount(1));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[0]);
  }

  // Move 4 under 3, this should expose 4 to the client.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(1, 3),
                        CreateNodeId(1, 4),
                        3));
    std::vector<Change> changes =
        background_connection_->DoRunLoopUntilChangesCount(1);
    Changes change_strings(ChangesToDescription1(changes));
    ASSERT_EQ(1u, change_strings.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,4 new_parent=1,3 "
        "old_parent=null", change_strings[0]);
    EXPECT_EQ("[node=1,4 parent=1,3 view=null]",
              ChangeNodeDescription(changes));
  }

  // Move 4 under 2, since 2 isn't a root client should get a delete.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(1, 2),
                        CreateNodeId(1, 4),
                        4));
    Changes changes = ChangesToDescription1(
        background_connection_->DoRunLoopUntilChangesCount(1));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=4 node=1,4", changes[0]);
  }

  // Delete 4, client shouldn't receive a delete since it should no longer know
  // about 4.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 4)));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    Changes changes = ChangesToDescription1(
        background_connection_->DoRunLoopUntilChangesCount(1));
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 6", changes[0]);
  }
}

// Verify AddNode fails when trying to manipulate nodes in other roots.
TEST_F(ViewManagerConnectionTest, CantMoveNodesFromOtherRoot) {
  // Create 1 and 2 in the first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_NO_FATAL_FAILURE(EstablishBackgroundConnectionWithRoot1());

  // Try to move 2 to be a child of 1 from connection 2. This should fail as 2
  // should not be able to access 1.
  ASSERT_FALSE(background_connection_->AddNode(
                   CreateNodeId(1, 1), CreateNodeId(1, 2), 1));

  // Try to reparent 1 to the root. A connection is not allowed to reparent its
  // roots.
  ASSERT_FALSE(background_connection_->AddNode(CreateNodeId(0, 1),
                                               CreateNodeId(1, 1), 1));
}

// Verify RemoveNodeFromParent fails for nodes that are descendants of the
// roots.
TEST_F(ViewManagerConnectionTest, CantRemoveNodesInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(EstablishBackgroundConnectionWithRoot1());

  // Connection 2 should not be able to remove node 2 or 1 from its parent.
  ASSERT_FALSE(background_connection_->RemoveNodeFromParent(
                   CreateNodeId(1, 2), 3));
  ASSERT_FALSE(background_connection_->RemoveNodeFromParent(CreateNodeId(1, 1),
                                                            3));

  // Create nodes 10 and 11 in 2.
  ASSERT_TRUE(background_connection_->CreateNode(CreateNodeId(2, 10)));
  ASSERT_TRUE(background_connection_->CreateNode(CreateNodeId(2, 11)));

  // Parent 11 to 10.
  ASSERT_TRUE(background_connection_->AddNode(CreateNodeId(2, 10),
                                              CreateNodeId(2, 11), 3));
  // Remove 11 from 10.
  ASSERT_TRUE(background_connection_->RemoveNodeFromParent(
                  CreateNodeId(2, 11), 4));

  // Verify nothing was actually removed.
  {
    std::vector<TestNode> nodes;
    GetNodeTree(view_manager_.get(), CreateNodeId(0, 1), &nodes);
    ASSERT_EQ(3u, nodes.size());
    EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[1].ToString());
    EXPECT_EQ("node=1,2 parent=0,1 view=null", nodes[2].ToString());
  }
}

// Verify SetView fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, CantRemoveSetViewInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));

  ASSERT_NO_FATAL_FAILURE(EstablishBackgroundConnectionWithRoot1());

  // Create a view in the second connection.
  ASSERT_TRUE(background_connection_->CreateView(CreateViewId(2, 51)));

  // Connection 2 should be able to set the view on node 1 (it's root), but not
  // on 2.
  ASSERT_TRUE(background_connection_->SetView(CreateNodeId(client_.id(), 1),
                                              CreateViewId(2, 51)));
  ASSERT_FALSE(background_connection_->SetView(CreateNodeId(client_.id(), 2),
                                               CreateViewId(2, 51)));
}

// Verify GetNodeTree fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, CantGetNodeTreeOfOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));

  ASSERT_NO_FATAL_FAILURE(EstablishBackgroundConnectionWithRoot1());

  std::vector<TestNode> nodes;

  // Should get nothing for the root.
  background_connection_->GetNodeTree(CreateNodeId(0, 1), &nodes);
  ASSERT_TRUE(nodes.empty());

  // Should get nothing for node 2.
  background_connection_->GetNodeTree(CreateNodeId(1, 2), &nodes);
  ASSERT_TRUE(nodes.empty());

  // Should get node 1 if asked for.
  background_connection_->GetNodeTree(CreateNodeId(1, 1), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ("node=1,1 parent=null view=null", nodes[0].ToString());
}

// Verify GetNodeTree fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, Connect) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(Connect(view_manager_.get(), kTestServiceURL, CreateNodeId(1, 1),
                      0));
  BackgroundConnection* instance = BackgroundConnection::WaitForInstance();
  ASSERT_TRUE(instance != NULL);
  Changes changes(
      ChangesToDescription1(instance->DoRunLoopUntilChangesCount(1)));;
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ("OnConnectionEstablished", changes[0]);
}

// TODO(sky): add coverage of test that destroys connections and ensures other
// connections get deletion notification (or advanced server id).

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
