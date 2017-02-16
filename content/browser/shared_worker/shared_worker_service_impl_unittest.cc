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
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_sync_message.h"
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
static const unsigned long long kDocumentIDs[] = {200, 201, 202};
static const int kRenderFrameRouteIDs[] = {300, 301, 302};

void BlockingReadFromMessagePort(MessagePort port, base::string16* message) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  port.SetCallback(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));
  event.Wait();

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
      : SharedWorkerMessageFilter(render_process_id,
                                  resource_context,
                                  partition,
                                  callback),
        message_queue_(message_queue) {}

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

 private:
  const int process_id_;
  std::vector<std::unique_ptr<IPC::Message>> queued_messages_;
  base::AtomicSequenceNumber next_routing_id_;
  scoped_refptr<MockSharedWorkerMessageFilter> worker_filter_;
};

void PostCreateWorker(MockRendererProcessHost* renderer,
                      const std::string& url,
                      const std::string& name,
                      unsigned long long document_id,
                      int render_frame_route_id,
                      ViewHostMsg_CreateWorker_Reply* reply) {
  ViewHostMsg_CreateWorker_Params params;
  params.url = GURL(url);
  params.name = base::ASCIIToUTF16(name);
  params.content_security_policy = base::string16();
  params.security_policy_type = blink::WebContentSecurityPolicyTypeReport;
  params.document_id = document_id;
  params.render_frame_route_id = render_frame_route_id;
  params.creation_context_type =
      blink::WebSharedWorkerCreationContextTypeSecure;
  EXPECT_TRUE(
      renderer->OnMessageReceived(new ViewHostMsg_CreateWorker(params, reply)));
}

class MockSharedWorkerConnector {
 public:
  MockSharedWorkerConnector(MockRendererProcessHost* renderer_host)
      : renderer_host_(renderer_host) {}
  void Create(const std::string& url,
              const std::string& name,
              unsigned long long document_id,
              int render_frame_route_id) {
    PostCreateWorker(renderer_host_, url, name, document_id,
                     render_frame_route_id, &create_worker_reply_);
  }
  void SendConnect() {
    mojo::MessagePipe message_pipe;
    local_port_ = MessagePort(std::move(message_pipe.handle0));

    EXPECT_TRUE(
        renderer_host_->OnMessageReceived(new ViewHostMsg_ConnectToWorker(
            create_worker_reply_.route_id,
            MessagePort(std::move(message_pipe.handle1)))));
  }
  MessagePort local_port() { return local_port_; }
  int route_id() { return create_worker_reply_.route_id; }
  blink::WebWorkerCreationError creation_error() {
    return create_worker_reply_.error;
  }
 private:
  MockRendererProcessHost* renderer_host_;
  MessagePort local_port_;
  ViewHostMsg_CreateWorker_Reply create_worker_reply_;
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

void CheckViewMsgWorkerCreated(MockRendererProcessHost* renderer_host,
                               MockSharedWorkerConnector* connector) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_WorkerCreated::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
}

void CheckViewMsgCountFeature(MockRendererProcessHost* renderer_host,
                              MockSharedWorkerConnector* connector,
                              uint32_t expected_feature) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_CountFeatureOnSharedWorker::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
  ViewMsg_CountFeatureOnSharedWorker::Param params;
  EXPECT_TRUE(ViewMsg_CountFeatureOnSharedWorker::Read(msg.get(), &params));
  uint32_t feature = std::get<0>(params);
  EXPECT_EQ(expected_feature, feature);
}

void CheckWorkerMsgConnect(MockRendererProcessHost* renderer_host,
                           int expected_msg_route_id,
                           int* connection_request_id,
                           MessagePort* port) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerMsg_Connect::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
  WorkerMsg_Connect::Param params;
  EXPECT_TRUE(WorkerMsg_Connect::Read(msg.get(), &params));
  *connection_request_id = std::get<0>(params);
  *port = std::get<1>(params);
}

void CheckViewMsgWorkerConnected(MockRendererProcessHost* renderer_host,
                                 MockSharedWorkerConnector* connector,
                                 std::set<uint32_t> expected_used_features) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_WorkerConnected::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
  ViewMsg_WorkerConnected::Param params;
  EXPECT_TRUE(ViewMsg_WorkerConnected::Read(msg.get(), &params));
  std::set<uint32_t> used_features = std::get<0>(params);
  EXPECT_EQ(expected_used_features, used_features);
}

}  // namespace

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<MockRendererProcessHost> renderer_host(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector(
      new MockSharedWorkerConnector(renderer_host.get()));
  int worker_route_id;

  // Sends ViewHostMsg_CreateWorker.
  connector->Create("http://example.com/w.js",
                    "name",
                    kDocumentIDs[0],
                    kRenderFrameRouteIDs[0]);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(renderer_host.get(),
                                    "http://example.com/w.js",
                                    "name",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  // ViewMsg_WorkerCreated(1) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host.get(), connector.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(1), it sends
  // WorkerMsg_Connect via ViewHostMsg_ConnectToWorker.
  connector->SendConnect();
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
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
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host.get(), connector.get(),
                              std::set<uint32_t>());

  // Verify that |worker_msg_port| corresponds to |connector->local_port()|.
  base::string16 expected_message(base::ASCIIToUTF16("test1"));
  connector->local_port().PostMessage(expected_message,
                                      std::vector<MessagePort>());
  base::string16 received_message;
  BlockingReadFromMessagePort(worker_msg_port, &received_message);
  EXPECT_EQ(expected_message, received_message);

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature().
  uint32_t feature1 = 124;
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // ViewMsg_CountFeature should be sent to SharedWorkerConnector side.
  CheckViewMsgCountFeature(renderer_host.get(), connector.get(), feature1);
  // A message should be sent only one time per feature.
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature() for another feature use.
  uint32_t feature2 = 901;
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature2)));
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // ViewMsg_CountFeature should be sent to SharedWorkerConnector side.
  CheckViewMsgCountFeature(renderer_host.get(), connector.get(), feature2);

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
  int worker_route_id;

  // Sends ViewHostMsg_CreateWorker.
  connector0->Create("http://example.com/w.js",
                     "name",
                     kDocumentIDs[0],
                     kRenderFrameRouteIDs[0]);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                    "http://example.com/w.js",
                                    "name",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  // ViewMsg_WorkerCreated(1) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(1), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector0->SendConnect();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
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
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host0.get(), connector0.get(),
                              std::set<uint32_t>());

  // Verify that |worker_msg_port1| corresponds to |connector0->local_port()|.
  base::string16 expected_message1(base::ASCIIToUTF16("test1"));
  connector0->local_port().PostMessage(expected_message1,
                                       std::vector<MessagePort>());
  base::string16 received_message1;
  BlockingReadFromMessagePort(worker_msg_port1, &received_message1);
  EXPECT_EQ(expected_message1, received_message1);

  // SharedWorker sends WorkerHostMsg_CountFeature in
  // EmbeddedSharedWorkerStub::CountFeature().
  uint32_t feature1 = 124;
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature1)));
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // ViewMsg_CountFeature should be sent to SharedWorkerConnector side.
  CheckViewMsgCountFeature(renderer_host0.get(), connector0.get(), feature1);
  uint32_t feature2 = 901;
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_CountFeature(worker_route_id, feature2)));
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // ViewMsg_CountFeature should be sent to SharedWorkerConnector side.
  CheckViewMsgCountFeature(renderer_host0.get(), connector0.get(), feature2);

  // The second renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));

  // UpdateWorkerDependency should not be called yet.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector1->Create("http://example.com/w.js",
                     "name",
                     kDocumentIDs[1],
                     kRenderFrameRouteIDs[1]);
  // We need to go to UI thread to call ReserveRenderProcessOnUI().
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // ViewMsg_WorkerCreated(3) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  // UpdateWorkerDependency should be called.
  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  EXPECT_EQ(1U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(kProcessIDs[0], s_worker_dependency_added_ids_[0]);
  EXPECT_EQ(0U, s_worker_dependency_removed_ids_.size());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(3), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector1->SendConnect();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
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
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host1.get(), connector1.get(),
                              {feature1, feature2});

  // Verify that |worker_msg_port2| corresponds to |connector1->local_port()|.
  base::string16 expected_message2(base::ASCIIToUTF16("test2"));
  connector1->local_port().PostMessage(expected_message2,
                                       std::vector<MessagePort>());
  base::string16 received_message2;
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
  // ViewMsg_CountFeature should be sent to all SharedWorkerConnectors
  // connecting to this worker.
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckViewMsgCountFeature(renderer_host0.get(), connector0.get(), feature3);
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckViewMsgCountFeature(renderer_host1.get(), connector1.get(), feature3);

  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  renderer_host1.reset();
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
  int worker_route_id;

  // Normal case.
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w1.js",
                       "name1",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w1.js",
                                      "name1",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    connector1->Create("http://example.com/w1.js",
                       "name1",
                       kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Normal case (URL mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w2.js",
                       "name2",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w2.js",
                                      "name2",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    connector1->Create("http://example.com/w2x.js",
                       "name2",
                       kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(blink::WebWorkerCreationErrorNone, connector1->creation_error());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w2x.js", "name2",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Normal case (name mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w3.js",
                       "name3",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w3.js", "name3",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    connector1->Create("http://example.com/w3.js", "name3x", kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(blink::WebWorkerCreationErrorNone, connector1->creation_error());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w3.js", "name3x",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Pending case.
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w4.js",
                       "name4",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    connector1->Create("http://example.com/w4.js", "name4", kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w4.js",
                                      "name4",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Pending case (URL mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w5.js", "name5", kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    connector1->Create("http://example.com/w5x.js", "name5", kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w5.js", "name5",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w5x.js", "name5",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Pending case (name mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w6.js", "name6", kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    connector1->Create("http://example.com/w6.js", "name6x", kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host0.get(), "http://example.com/w6.js", "name6",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(
        renderer_host1.get(), "http://example.com/w6.js", "name6x",
        blink::WebContentSecurityPolicyTypeReport, &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }
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
  connector0->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[0],
                     kRenderFrameRouteIDs[0]);
  EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                    "http://example.com/w1.js",
                                    "name1",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
  renderer_host0->FastShutdownIfPossible();

  connector1->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[1],
                     kRenderFrameRouteIDs[1]);
  EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(renderer_host1.get(),
                                    "http://example.com/w1.js",
                                    "name1",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  connector2->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[2],
                     kRenderFrameRouteIDs[2]);
  EXPECT_NE(MSG_ROUTING_NONE, connector2->route_id());
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  CheckViewMsgWorkerCreated(renderer_host2.get(), connector2.get());
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
  connector0->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[0],
                     kRenderFrameRouteIDs[0]);
  EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  renderer_host0->FastShutdownIfPossible();

  connector1->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[1],
                     kRenderFrameRouteIDs[1]);
  EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(renderer_host1.get(),
                                    "http://example.com/w1.js",
                                    "name1",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  connector2->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[2],
                     kRenderFrameRouteIDs[2]);
  EXPECT_NE(MSG_ROUTING_NONE, connector2->route_id());
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  CheckViewMsgWorkerCreated(renderer_host2.get(), connector2.get());
}

}  // namespace content
