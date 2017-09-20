// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "base/atomic_sequence_num.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "content/browser/shared_worker/shared_worker_connector_impl.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_sync_message.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SharedWorkerServiceImplTest : public testing::Test {
 public:
  static void RegisterRunningProcessID(int process_id) {
    base::AutoLock lock(s_lock_);
    s_running_process_id_set_.insert(process_id);
  }
  static void UnregisterRunningProcessID(int process_id) {
    base::AutoLock lock(s_lock_);
    s_running_process_id_set_.erase(process_id);
  }

  static void CreateSharedWorkerConnector(
      int process_id,
      int frame_id,
      ResourceContext* resource_context,
      WorkerStoragePartition partition,
      mojom::SharedWorkerConnectorRequest request) {
    SharedWorkerConnectorImpl::CreateOnIOThread(
        process_id, frame_id, resource_context, partition, std::move(request));
  }

 protected:
  SharedWorkerServiceImplTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()),
        partition_(new WorkerStoragePartition(
            BrowserContext::GetDefaultStoragePartition(browser_context_.get())
                ->GetURLRequestContext(),
            nullptr /* media_url_request_context */,
            nullptr /* appcache_service */,
            nullptr /* quota_manager */,
            nullptr /* filesystem_context */,
            nullptr /* database_tracker */,
            nullptr /* indexed_db_context */,
            nullptr /* service_worker_context */)) {
    SharedWorkerServiceImpl::GetInstance()
        ->ChangeUpdateWorkerDependencyFuncForTesting(
            &SharedWorkerServiceImplTest::MockUpdateWorkerDependency);
    SharedWorkerServiceImpl::GetInstance()
        ->ChangeTryIncrementWorkerRefCountFuncForTesting(
            &SharedWorkerServiceImplTest::MockTryIncrementWorkerRefCount);
  }

  void SetUp() override {}
  void TearDown() override {
    s_update_worker_dependency_call_count_ = 0;
    s_worker_dependency_added_ids_.clear();
    s_worker_dependency_removed_ids_.clear();
    s_running_process_id_set_.clear();
    SharedWorkerServiceImpl::GetInstance()->ResetForTesting();
  }
  static void MockUpdateWorkerDependency(const std::vector<int>& added_ids,
                                         const std::vector<int>& removed_ids) {
    ++s_update_worker_dependency_call_count_;
    s_worker_dependency_added_ids_ = added_ids;
    s_worker_dependency_removed_ids_ = removed_ids;
  }
  static bool MockTryIncrementWorkerRefCount(int worker_process_id) {
    base::AutoLock lock(s_lock_);
    return s_running_process_id_set_.find(worker_process_id) !=
           s_running_process_id_set_.end();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<WorkerStoragePartition> partition_;
  static int s_update_worker_dependency_call_count_;
  static std::vector<int> s_worker_dependency_added_ids_;
  static std::vector<int> s_worker_dependency_removed_ids_;
  static base::Lock s_lock_;
  static std::set<int> s_running_process_id_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

// static
int SharedWorkerServiceImplTest::s_update_worker_dependency_call_count_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_added_ids_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_removed_ids_;
base::Lock SharedWorkerServiceImplTest::s_lock_;
std::set<int> SharedWorkerServiceImplTest::s_running_process_id_set_;

namespace {

static const int kProcessIDs[] = {100, 101, 102};
static const int kRenderFrameRouteIDs[] = {300, 301, 302};

std::vector<uint8_t> StringPieceToVector(base::StringPiece s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

void BlockingReadFromMessagePort(MessagePort port,
                                 std::vector<uint8_t>* message) {
  base::RunLoop run_loop;
  port.SetCallback(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<MessagePort> should_be_empty;
  EXPECT_TRUE(port.GetMessage(message, &should_be_empty));
  EXPECT_TRUE(should_be_empty.empty());
}

class MockSharedWorkerMessageFilter : public SharedWorkerMessageFilter {
 public:
  MockSharedWorkerMessageFilter(
      int render_process_id,
      ResourceContext* resource_context,
      const WorkerStoragePartition& partition,
      const SharedWorkerMessageFilter::NextRoutingIDCallback& callback,
      std::vector<std::unique_ptr<IPC::Message>>* message_queue)
      : SharedWorkerMessageFilter(render_process_id, callback),
        message_queue_(message_queue) {
    OnFilterAdded(nullptr);
  }

  bool Send(IPC::Message* message) override {
    std::unique_ptr<IPC::Message> owned(message);
    if (!message_queue_)
      return false;
    message_queue_->push_back(std::move(owned));
    return true;
  }

  void Close() {
    message_queue_ = nullptr;
    OnChannelClosing();
    OnFilterRemoved();
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return SharedWorkerMessageFilter::OnMessageReceived(message);
  }

 private:
  ~MockSharedWorkerMessageFilter() override {}
  std::vector<std::unique_ptr<IPC::Message>>* message_queue_;
};

class MockRendererProcessHost {
 public:
  MockRendererProcessHost(int process_id,
                          ResourceContext* resource_context,
                          const WorkerStoragePartition& partition)
      : process_id_(process_id),
        resource_context_(resource_context),
        partition_(partition),
        worker_filter_(new MockSharedWorkerMessageFilter(
            process_id,
            resource_context,
            partition,
            base::Bind(&base::AtomicSequenceNumber::GetNext,
                       base::Unretained(&next_routing_id_)),
            &queued_messages_)) {
    SharedWorkerServiceImplTest::RegisterRunningProcessID(process_id);
  }

  ~MockRendererProcessHost() {
    SharedWorkerServiceImplTest::UnregisterRunningProcessID(process_id_);
    worker_filter_->Close();
  }

  bool OnMessageReceived(IPC::Message* message) {
    std::unique_ptr<IPC::Message> msg(message);
    const bool ret = worker_filter_->OnMessageReceived(*message);
    if (message->is_sync()) {
      CHECK(!queued_messages_.empty());
      std::unique_ptr<IPC::Message> response_msg(
          queued_messages_.back().release());
      queued_messages_.pop_back();
      IPC::SyncMessage* sync_msg = static_cast<IPC::SyncMessage*>(message);
      std::unique_ptr<IPC::MessageReplyDeserializer> reply_serializer(
          sync_msg->GetReplyDeserializer());
      bool result = reply_serializer->SerializeOutputParameters(*response_msg);
      CHECK(result);
    }
    return ret;
  }

  size_t QueuedMessageCount() const { return queued_messages_.size(); }

  std::unique_ptr<IPC::Message> PopMessage() {
    CHECK(queued_messages_.size());
    std::unique_ptr<IPC::Message> msg(queued_messages_.begin()->release());
    queued_messages_.erase(queued_messages_.begin());
    return msg;
  }

  void FastShutdownIfPossible() {
    SharedWorkerServiceImplTest::UnregisterRunningProcessID(process_id_);
  }

  void CreateSharedWorkerConnector(
      int frame_id,
      mojom::SharedWorkerConnectorRequest request) {
    SharedWorkerServiceImplTest::CreateSharedWorkerConnector(
        process_id_, frame_id, resource_context_, partition_,
        std::move(request));
  }

 private:
  const int process_id_;
  ResourceContext* resource_context_;
  WorkerStoragePartition partition_;
  std::vector<std::unique_ptr<IPC::Message>> queued_messages_;
  base::AtomicSequenceNumber next_routing_id_;
  scoped_refptr<MockSharedWorkerMessageFilter> worker_filter_;
};

class MockSharedWorkerClient : public mojom::SharedWorkerClient {
 public:
  MockSharedWorkerClient() : binding_(this) {}

  void Bind(mojom::SharedWorkerClientRequest request) {
    binding_.Bind(std::move(request));
  }

  void CheckReceivedOnCreated() { EXPECT_TRUE(on_created_received_); }

  void CheckReceivedOnConnected(std::set<uint32_t> expected_used_features) {
    EXPECT_TRUE(on_connected_received_);
    EXPECT_EQ(expected_used_features, on_connected_features_);
  }

  void CheckReceivedOnFeatureUsed(uint32_t feature) {
    EXPECT_TRUE(on_feature_used_received_);
    EXPECT_EQ(feature, on_feature_used_feature_);
  }

 private:
  // mojom::SharedWorkerClient methods:
  void OnCreated(blink::mojom::SharedWorkerCreationContextType
                     creation_context_type) override {
    on_created_received_ = true;
  }
  void OnConnected(
      const std::vector<blink::mojom::WebFeature>& features_used) override {
    on_connected_received_ = true;
    for (auto feature : features_used)
      on_connected_features_.insert(static_cast<uint32_t>(feature));
  }
  void OnScriptLoadFailed() override {}
  void OnFeatureUsed(blink::mojom::WebFeature feature) override {
    on_feature_used_received_ = true;
    on_feature_used_feature_ = static_cast<uint32_t>(feature);
  }

  mojo::Binding<mojom::SharedWorkerClient> binding_;
  bool on_created_received_ = false;
  bool on_connected_received_ = false;
  bool on_feature_used_received_ = false;
  std::set<uint32_t> on_connected_features_;
  uint32_t on_feature_used_feature_ = 0;
};

class MockSharedWorkerConnector {
 public:
  MockSharedWorkerConnector(MockRendererProcessHost* renderer_host)
      : renderer_host_(renderer_host) {}
  void Create(const std::string& url,
              const std::string& name,
              int render_frame_route_id,
              MockSharedWorkerClient* client) {
    mojom::SharedWorkerConnectorPtr connector;
    renderer_host_->CreateSharedWorkerConnector(render_frame_route_id,
                                                mojo::MakeRequest(&connector));

    mojom::SharedWorkerInfoPtr info(mojom::SharedWorkerInfo::New(
        GURL(url), name, std::string(),
        blink::kWebContentSecurityPolicyTypeReport,
        blink::kWebAddressSpacePublic,
        blink::mojom::SharedWorkerCreationContextType::kSecure,
        false /* data_saver_enabled */));

    mojo::MessagePipe message_pipe;
    local_port_ = MessagePort(std::move(message_pipe.handle0));

    mojom::SharedWorkerClientPtr client_proxy;
    client->Bind(mojo::MakeRequest(&client_proxy));

    connector->Connect(std::move(info), std::move(client_proxy),
                       std::move(message_pipe.handle1));
  }
  MessagePort local_port() { return local_port_; }
 private:
  mojom::SharedWorkerClientRequest client_request_;
  MockRendererProcessHost* renderer_host_;
  MessagePort local_port_;
};

void CheckWorkerProcessMsgCreateWorker(
    MockRendererProcessHost* renderer_host,
    const std::string& expected_url,
    const std::string& expected_name,
    blink::WebContentSecurityPolicyType expected_security_policy_type,
    int* route_id) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerProcessMsg_CreateWorker::ID, msg->type());
  std::tuple<WorkerProcessMsg_CreateWorker_Params> param;
  EXPECT_TRUE(WorkerProcessMsg_CreateWorker::Read(msg.get(), &param));
  EXPECT_EQ(GURL(expected_url), std::get<0>(param).url);
  EXPECT_EQ(base::ASCIIToUTF16(expected_name), std::get<0>(param).name);
  EXPECT_EQ(expected_security_policy_type,
            std::get<0>(param).security_policy_type);
  *route_id = std::get<0>(param).route_id;
}

void CheckWorkerMsgConnect(MockRendererProcessHost* renderer_host,
                           int expected_msg_route_id,
                           int* connection_request_id = nullptr,
                           MessagePort* port = nullptr) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerMsg_Connect::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
  WorkerMsg_Connect::Param params;
  EXPECT_TRUE(WorkerMsg_Connect::Read(msg.get(), &params));
  if (connection_request_id)
    *connection_request_id = std::get<0>(params);
  if (port)
    *port = std::get<1>(params);
}

void CheckWorkerMsgTerminateWorkerContext(
    MockRendererProcessHost* renderer_host,
    int expected_msg_route_id) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerMsg_TerminateWorkerContext::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
}

}  // namespace

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<MockRendererProcessHost> renderer_host(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector(
      new MockSharedWorkerConnector(renderer_host.get()));
  MockSharedWorkerClient client;
  int worker_route_id;

  // Sends ViewHostMsg_CreateWorker.
  connector->Create("http://example.com/w.js", "name", kRenderFrameRouteIDs[0],
                    &client);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(
      renderer_host.get(), "http://example.com/w.js", "name",
      blink::kWebContentSecurityPolicyTypeReport, &worker_route_id);
  client.CheckReceivedOnCreated();

  // WorkerMsg_Connect should be sent to SharedWorker side.
  int worker_msg_connection_request_id;
  MessagePort worker_msg_port;
  CheckWorkerMsgConnect(renderer_host.get(), worker_route_id,
                        &worker_msg_connection_request_id, &worker_msg_port);

  // SharedWorker sends WorkerHostMsg_WorkerReadyForInspection in
  // EmbeddedSharedWorkerStub::WorkerReadyForInspection().
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_WorkerReadyForInspection(worker_route_id)));
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerScriptLoaded in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_WorkerScriptLoaded(worker_route_id)));
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(
      renderer_host->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          worker_msg_connection_request_id, worker_route_id)));
  RunAllPendingInMessageLoop();
  client.CheckReceivedOnConnected(std::set<uint32_t>());

  // Verify that |worker_msg_port| corresponds to |connector->local_port()|.
  std::vector<uint8_t> expected_message(StringPieceToVector("test1"));
  connector->local_port().PostMessage(expected_message.data(),
                                      expected_message.size(),
                                      std::vector<MessagePort>());
  std::vector<uint8_t> received_message;
  BlockingReadFromMessagePort(worker_msg_port, &received_message);
  EXPECT_EQ(expected_message, received_message);

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature().
  uint32_t feature1 = 124;
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  RunAllPendingInMessageLoop();
  client.CheckReceivedOnFeatureUsed(feature1);
  // A message should be sent only one time per feature.
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature() for another feature use.
  uint32_t feature2 = 901;
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature2)));
  RunAllPendingInMessageLoop();
  client.CheckReceivedOnFeatureUsed(feature2);

  // UpdateWorkerDependency should not be called.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  int worker_route_id;

  // Sends ViewHostMsg_CreateWorker.
  connector0->Create("http://example.com/w.js", "name", kRenderFrameRouteIDs[0],
                     &client0);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(
      renderer_host0.get(), "http://example.com/w.js", "name",
      blink::kWebContentSecurityPolicyTypeReport, &worker_route_id);
  client0.CheckReceivedOnCreated();

  // WorkerMsg_Connect should be sent to SharedWorker side.
  int worker_msg_connection_request_id1;
  MessagePort worker_msg_port1;
  CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id,
                        &worker_msg_connection_request_id1, &worker_msg_port1);

  // SharedWorker sends WorkerHostMsg_WorkerReadyForInspection in
  // EmbeddedSharedWorkerStub::WorkerReadyForInspection().
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_WorkerReadyForInspection(worker_route_id)));
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerScriptLoaded in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_WorkerScriptLoaded(worker_route_id)));
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(
      renderer_host0->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          worker_msg_connection_request_id1, worker_route_id)));
  RunAllPendingInMessageLoop();
  client0.CheckReceivedOnConnected(std::set<uint32_t>());

  // Verify that |worker_msg_port1| corresponds to |connector0->local_port()|.
  std::vector<uint8_t> expected_message1(StringPieceToVector("test1"));
  connector0->local_port().PostMessage(expected_message1.data(),
                                       expected_message1.size(),
                                       std::vector<MessagePort>());
  std::vector<uint8_t> received_message1;
  BlockingReadFromMessagePort(worker_msg_port1, &received_message1);
  EXPECT_EQ(expected_message1, received_message1);

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature().
  uint32_t feature1 = 124;
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  RunAllPendingInMessageLoop();
  client0.CheckReceivedOnFeatureUsed(feature1);
  uint32_t feature2 = 901;
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature2)));
  RunAllPendingInMessageLoop();
  client0.CheckReceivedOnFeatureUsed(feature2);

  // The second renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;

  // UpdateWorkerDependency should not be called yet.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector1->Create("http://example.com/w.js", "name", kRenderFrameRouteIDs[1],
                     &client1);
  // We need to go to UI thread to call ReserveRenderProcessOnUI().
  RunAllPendingInMessageLoop();
  client1.CheckReceivedOnCreated();

  // UpdateWorkerDependency should be called.
  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  EXPECT_EQ(1U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(kProcessIDs[0], s_worker_dependency_added_ids_[0]);
  EXPECT_EQ(0U, s_worker_dependency_removed_ids_.size());

  // WorkerMsg_Connect should be sent to SharedWorker side.
  int worker_msg_connection_request_id2;
  MessagePort worker_msg_port2;
  CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id,
                        &worker_msg_connection_request_id2, &worker_msg_port2);

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::OnConnect().
  EXPECT_TRUE(
      renderer_host0->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          worker_msg_connection_request_id2, worker_route_id)));
  RunAllPendingInMessageLoop();
  client1.CheckReceivedOnConnected({feature1, feature2});

  // Verify that |worker_msg_port2| corresponds to |connector1->local_port()|.
  std::vector<uint8_t> expected_message2(StringPieceToVector("test2"));
  connector1->local_port().PostMessage(expected_message2.data(),
                                       expected_message2.size(),
                                       std::vector<MessagePort>());
  std::vector<uint8_t> received_message2;
  BlockingReadFromMessagePort(worker_msg_port2, &received_message2);
  EXPECT_EQ(expected_message2, received_message2);

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature(). These used_features are already
  // counted in the browser-side, so messages should not be sent to
  // SharedWorkerConnectors.
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature() for another feature use.
  uint32_t feature3 = 1019;
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature3)));
  RunAllPendingInMessageLoop();
  client0.CheckReceivedOnFeatureUsed(feature3);
  client1.CheckReceivedOnFeatureUsed(feature3);

  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  renderer_host1.reset();
  // UpdateWorkerDependency should NOT be called. The shared worker is still
  // alive.
  EXPECT_EQ(1, s_update_worker_dependency_call_count_);

  renderer_host0.reset();
  // UpdateWorkerDependency should be called.
  EXPECT_EQ(2, s_update_worker_dependency_call_count_);
  EXPECT_EQ(0U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(1U, s_worker_dependency_removed_ids_.size());
  EXPECT_EQ(kProcessIDs[0], s_worker_dependency_removed_ids_[0]);
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest) {
  // The first renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  // The second renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  int worker_route_id0, worker_route_id1;

  // Normal case.
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    MockSharedWorkerClient client0;
    connector0->Create("http://example.com/w1.js", "name1",
                       kRenderFrameRouteIDs[0], &client0);
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w1.js", "name1",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client0.CheckReceivedOnCreated();
    MockSharedWorkerClient client1;
    connector1->Create("http://example.com/w1.js", "name1",
                       kRenderFrameRouteIDs[1], &client1);
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client1.CheckReceivedOnCreated();
  }
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host0.get(), worker_route_id0);
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());

  // Normal case (URL mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    MockSharedWorkerClient client0;
    connector0->Create("http://example.com/w2.js", "name2",
                       kRenderFrameRouteIDs[0], &client0);
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w2.js", "name2",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client0.CheckReceivedOnCreated();
    MockSharedWorkerClient client1;
    connector1->Create("http://example.com/w2x.js", "name2",
                       kRenderFrameRouteIDs[1], &client1);
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w2x.js", "name2",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id1);
    CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id1);
    client1.CheckReceivedOnCreated();
    EXPECT_NE(worker_route_id0, worker_route_id1);
  }
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host0.get(), worker_route_id0);
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host1.get(), worker_route_id1);

  // Normal case (name mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    MockSharedWorkerClient client0;
    connector0->Create("http://example.com/w3.js", "name3",
                       kRenderFrameRouteIDs[0], &client0);
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w3.js", "name3",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client0.CheckReceivedOnCreated();
    MockSharedWorkerClient client1;
    connector1->Create("http://example.com/w3.js", "name3x",
                       kRenderFrameRouteIDs[1], &client1);
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w3.js", "name3x",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id1);
    CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id1);
    client1.CheckReceivedOnCreated();
  }
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host0.get(), worker_route_id0);
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host1.get(), worker_route_id1);

  // Pending case.
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    MockSharedWorkerClient client0;
    connector0->Create("http://example.com/w4.js", "name4",
                       kRenderFrameRouteIDs[0], &client0);
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    MockSharedWorkerClient client1;
    connector1->Create("http://example.com/w4.js", "name4",
                       kRenderFrameRouteIDs[1], &client1);
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(3U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w4.js", "name4",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client0.CheckReceivedOnCreated();
    client1.CheckReceivedOnCreated();
  }
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host0.get(), worker_route_id0);
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());

  // Pending case (URL mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    MockSharedWorkerClient client0;
    connector0->Create("http://example.com/w5.js", "name5",
                       kRenderFrameRouteIDs[0], &client0);
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    MockSharedWorkerClient client1;
    connector1->Create("http://example.com/w5x.js", "name5",
                       kRenderFrameRouteIDs[1], &client1);
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w5.js", "name5",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client0.CheckReceivedOnCreated();
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w5x.js", "name5",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id1);
    CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id1);
    client1.CheckReceivedOnCreated();
  }
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host0.get(), worker_route_id0);
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host1.get(), worker_route_id1);

  // Pending case (name mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    MockSharedWorkerClient client0;
    connector0->Create("http://example.com/w6.js", "name6",
                       kRenderFrameRouteIDs[0], &client0);
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    MockSharedWorkerClient client1;
    connector1->Create("http://example.com/w6.js", "name6x",
                       kRenderFrameRouteIDs[1], &client1);
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w6.js", "name6",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id0);
    CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id0);
    client0.CheckReceivedOnCreated();
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w6.js", "name6x",
        blink::kWebContentSecurityPolicyTypeReport, &worker_route_id1);
    CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id1);
    client1.CheckReceivedOnCreated();
  }
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host0.get(), worker_route_id0);
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckWorkerMsgTerminateWorkerContext(renderer_host1.get(), worker_route_id1);
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest) {
  // Create three renderer hosts.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host2(
      new MockRendererProcessHost(kProcessIDs[2],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  int worker_route_id;

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));
  MockSharedWorkerClient client0;
  connector0->Create("http://example.com/w1.js", "name1",
                     kRenderFrameRouteIDs[0], &client0);
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(
      renderer_host0.get(), "http://example.com/w1.js", "name1",
      blink::kWebContentSecurityPolicyTypeReport, &worker_route_id);
  CheckWorkerMsgConnect(renderer_host0.get(), worker_route_id);
  client0.CheckReceivedOnCreated();
  renderer_host0->FastShutdownIfPossible();

  MockSharedWorkerClient client1;
  connector1->Create("http://example.com/w1.js", "name1",
                     kRenderFrameRouteIDs[1], &client1);
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(
      renderer_host1.get(), "http://example.com/w1.js", "name1",
      blink::kWebContentSecurityPolicyTypeReport, &worker_route_id);
  CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id);
  client1.CheckReceivedOnCreated();

  MockSharedWorkerClient client2;
  connector2->Create("http://example.com/w1.js", "name1",
                     kRenderFrameRouteIDs[2], &client2);
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id);
  client2.CheckReceivedOnCreated();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest2) {
  // Create three renderer hosts.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host2(
      new MockRendererProcessHost(kProcessIDs[2],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  int worker_route_id;

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));
  MockSharedWorkerClient client0;
  connector0->Create("http://example.com/w1.js", "name1",
                     kRenderFrameRouteIDs[0], &client0);
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  renderer_host0->FastShutdownIfPossible();

  MockSharedWorkerClient client1;
  connector1->Create("http://example.com/w1.js", "name1",
                     kRenderFrameRouteIDs[1], &client1);
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(
      renderer_host1.get(), "http://example.com/w1.js", "name1",
      blink::kWebContentSecurityPolicyTypeReport, &worker_route_id);
  CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id);
  client1.CheckReceivedOnCreated();

  MockSharedWorkerClient client2;
  connector2->Create("http://example.com/w1.js", "name1",
                     kRenderFrameRouteIDs[2], &client2);
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckWorkerMsgConnect(renderer_host1.get(), worker_route_id);
  client2.CheckReceivedOnCreated();
}

}  // namespace content
