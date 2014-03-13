// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/browser/worker_host/worker_storage_partition.h"
#include "content/common/message_port_messages.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_sync_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MockMessagePortMessageFilter : public MessagePortMessageFilter {
 public:
  MockMessagePortMessageFilter(const NextRoutingIDCallback& callback,
                               ScopedVector<IPC::Message>* message_queue)
      : MessagePortMessageFilter(callback), message_queue_(message_queue) {}

  virtual bool Send(IPC::Message* message) OVERRIDE {
    if (!message_queue_) {
      delete message;
      return false;
    }
    message_queue_->push_back(message);
    return true;
  }

  void Close() {
    message_queue_ = NULL;
    OnChannelClosing();
  }

 private:
  virtual ~MockMessagePortMessageFilter() {}
  ScopedVector<IPC::Message>* message_queue_;
};

class MockSharedWorkerMessageFilter : public SharedWorkerMessageFilter {
 public:
  MockSharedWorkerMessageFilter(int render_process_id,
                                ResourceContext* resource_context,
                                const WorkerStoragePartition& partition,
                                MessagePortMessageFilter* message_port_filter,
                                ScopedVector<IPC::Message>* message_queue)
      : SharedWorkerMessageFilter(render_process_id,
                                  resource_context,
                                  partition,
                                  message_port_filter),
        message_queue_(message_queue) {}

  virtual bool Send(IPC::Message* message) OVERRIDE {
    if (!message_queue_) {
      delete message;
      return false;
    }
    message_queue_->push_back(message);
    return true;
  }

  void Close() {
    message_queue_ = NULL;
    OnChannelClosing();
  }

 private:
  virtual ~MockSharedWorkerMessageFilter() {}
  ScopedVector<IPC::Message>* message_queue_;
};

class MockRendererProcessHost {
 public:
  MockRendererProcessHost(int process_id,
                          ResourceContext* resource_context,
                          const WorkerStoragePartition& partition)
      : message_filter_(new MockMessagePortMessageFilter(
            base::Bind(&base::AtomicSequenceNumber::GetNext,
                       base::Unretained(&next_routing_id_)),
            &queued_messages_)),
        worker_filter_(new MockSharedWorkerMessageFilter(process_id,
                                                         resource_context,
                                                         partition,
                                                         message_filter_.get(),
                                                         &queued_messages_)) {}

  ~MockRendererProcessHost() {
    message_filter_->Close();
    worker_filter_->Close();
  }

  bool OnMessageReceived(IPC::Message* message) {
    scoped_ptr<IPC::Message> msg(message);
    bool message_was_ok = false;
    const bool ret =
        message_filter_->OnMessageReceived(*message, &message_was_ok) ||
        worker_filter_->OnMessageReceived(*message, &message_was_ok);
    if (message->is_sync()) {
      CHECK(!queued_messages_.empty());
      const IPC::Message* response_msg = queued_messages_.back();
      IPC::SyncMessage* sync_msg = static_cast<IPC::SyncMessage*>(message);
      scoped_ptr<IPC::MessageReplyDeserializer> reply_serializer(
          sync_msg->GetReplyDeserializer());
      bool result = reply_serializer->SerializeOutputParameters(*response_msg);
      CHECK(result);
      queued_messages_.pop_back();
    }
    return ret;
  }

  size_t QueuedMessageCount() const { return queued_messages_.size(); }

  scoped_ptr<IPC::Message> PopMessage() {
    CHECK(queued_messages_.size());
    scoped_ptr<IPC::Message> msg(*queued_messages_.begin());
    queued_messages_.weak_erase(queued_messages_.begin());
    return msg.Pass();
  }

 private:
  ScopedVector<IPC::Message> queued_messages_;
  base::AtomicSequenceNumber next_routing_id_;
  scoped_refptr<MockMessagePortMessageFilter> message_filter_;
  scoped_refptr<MockSharedWorkerMessageFilter> worker_filter_;
};

void CreateMessagePortPair(MockRendererProcessHost* renderer,
                           int* route_1,
                           int* port_1,
                           int* route_2,
                           int* port_2) {
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_CreateMessagePort(route_1, port_1)));
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_CreateMessagePort(route_2, port_2)));
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_Entangle(*port_1, *port_2)));
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_Entangle(*port_2, *port_1)));
}

void PostCreateWorker(MockRendererProcessHost* renderer,
                      const std::string& url,
                      const std::string& name,
                      unsigned long long document_id,
                      int render_frame_route_id,
                      int* connector_route_id) {
  ViewHostMsg_CreateWorker_Params params;
  params.url = GURL(url);
  params.name = base::ASCIIToUTF16(name);
  params.content_security_policy = base::string16();
  params.security_policy_type = blink::WebContentSecurityPolicyTypeReport;
  params.document_id = document_id;
  params.render_frame_route_id = render_frame_route_id;
  EXPECT_TRUE(renderer->OnMessageReceived(
      new ViewHostMsg_CreateWorker(params, connector_route_id)));
}

class MockSharedWorkerConnector {
 public:
  MockSharedWorkerConnector(MockRendererProcessHost* renderer_host)
      : renderer_host_(renderer_host),
        temporary_remote_port_route_id_(0),
        remote_port_id_(0),
        local_port_route_id_(0),
        local_port_id_(0),
        route_id_(0) {}
  void Create(const std::string& url,
              const std::string& name,
              unsigned long long document_id,
              int render_frame_route_id) {
    CreateMessagePortPair(renderer_host_,
                          &temporary_remote_port_route_id_,
                          &remote_port_id_,
                          &local_port_route_id_,
                          &local_port_id_);
    PostCreateWorker(renderer_host_,
                     url,
                     name,
                     document_id,
                     render_frame_route_id,
                     &route_id_);
  }
  void SendQueueMessages() {
    EXPECT_TRUE(renderer_host_->OnMessageReceived(
        new MessagePortHostMsg_QueueMessages(remote_port_id_)));
  }
  void SendPostMessage(std::string data) {
    const std::vector<int> empty_ids;
    EXPECT_TRUE(
        renderer_host_->OnMessageReceived(new MessagePortHostMsg_PostMessage(
            local_port_id_, base::ASCIIToUTF16(data), empty_ids)));
  }
  void SendConnect() {
    EXPECT_TRUE(
        renderer_host_->OnMessageReceived(new ViewHostMsg_ForwardToWorker(
            WorkerMsg_Connect(route_id_, remote_port_id_, MSG_ROUTING_NONE))));
  }
  void SendSendQueuedMessages(
      const std::vector<QueuedMessage>& queued_messages) {
    EXPECT_TRUE(renderer_host_->OnMessageReceived(
        new MessagePortHostMsg_SendQueuedMessages(remote_port_id_,
                                                  queued_messages)));
  }
  int temporary_remote_port_route_id() {
    return temporary_remote_port_route_id_;
  }
  int remote_port_id() { return remote_port_id_; }
  int local_port_route_id() { return local_port_route_id_; }
  int local_port_id() { return local_port_id_; }
  int route_id() { return route_id_; }

 private:
  MockRendererProcessHost* renderer_host_;
  int temporary_remote_port_route_id_;
  int remote_port_id_;
  int local_port_route_id_;
  int local_port_id_;
  int route_id_;
};

void CheckWorkerProcessMsgCreateWorker(
    MockRendererProcessHost* renderer_host,
    const std::string& expected_url,
    const std::string& expected_name,
    blink::WebContentSecurityPolicyType expected_security_policy_type,
    int* route_id) {
  scoped_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerProcessMsg_CreateWorker::ID, msg->type());
  Tuple1<WorkerProcessMsg_CreateWorker_Params> param;
  EXPECT_TRUE(WorkerProcessMsg_CreateWorker::Read(msg.get(), &param));
  EXPECT_EQ(GURL(expected_url), param.a.url);
  EXPECT_EQ(base::ASCIIToUTF16(expected_name), param.a.name);
  EXPECT_EQ(expected_security_policy_type, param.a.security_policy_type);
  *route_id = param.a.route_id;
}

void CheckViewMsgWorkerCreated(MockRendererProcessHost* renderer_host,
                               MockSharedWorkerConnector* connector) {
  scoped_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_WorkerCreated::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
}

void CheckMessagePortMsgMessagesQueued(MockRendererProcessHost* renderer_host,
                                       MockSharedWorkerConnector* connector) {
  scoped_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(MessagePortMsg_MessagesQueued::ID, msg->type());
  EXPECT_EQ(connector->temporary_remote_port_route_id(), msg->routing_id());
}

void CheckWorkerMsgConnect(MockRendererProcessHost* renderer_host,
                           int expected_msg_route_id,
                           int expected_sent_message_port_id,
                           int* routing_id) {
  scoped_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerMsg_Connect::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
  int port_id;
  EXPECT_TRUE(WorkerMsg_Connect::Read(msg.get(), &port_id, routing_id));
  EXPECT_EQ(expected_sent_message_port_id, port_id);
}

void CheckMessagePortMsgMessage(MockRendererProcessHost* renderer_host,
                                int expected_msg_route_id,
                                std::string expected_data) {
  scoped_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(MessagePortMsg_Message::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
  base::string16 data;
  std::vector<int> sent_message_port_ids;
  std::vector<int> new_routing_ids;
  EXPECT_TRUE(MessagePortMsg_Message::Read(
      msg.get(), &data, &sent_message_port_ids, &new_routing_ids));
  EXPECT_EQ(base::ASCIIToUTF16(expected_data), data);
}

void CheckViewMsgWorkerConnected(MockRendererProcessHost* renderer_host,
                                 MockSharedWorkerConnector* connector) {
  scoped_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_WorkerConnected::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
}

}  // namespace

class SharedWorkerServiceImplTest : public testing::Test {
 protected:
  SharedWorkerServiceImplTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()),
        partition_(
            new WorkerStoragePartition(browser_context_->GetRequestContext(),
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)) {
    SharedWorkerServiceImpl::GetInstance()
        ->ChangeUpdateWorkerDependencyFuncForTesting(
            &SharedWorkerServiceImplTest::MockUpdateWorkerDependency);
  }

  virtual void SetUp() OVERRIDE {}
  virtual void TearDown() OVERRIDE {
    s_update_worker_dependency_call_count_ = 0;
    s_worker_dependency_added_ids_.clear();
    s_worker_dependency_removed_ids_.clear();
    SharedWorkerServiceImpl::GetInstance()->ResetForTesting();
  }
  static void MockUpdateWorkerDependency(const std::vector<int>& added_ids,
                                         const std::vector<int>& removed_ids) {
    ++s_update_worker_dependency_call_count_;
    s_worker_dependency_added_ids_ = added_ids;
    s_worker_dependency_removed_ids_ = removed_ids;
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<WorkerStoragePartition> partition_;
  static int s_update_worker_dependency_call_count_;
  static std::vector<int> s_worker_dependency_added_ids_;
  static std::vector<int> s_worker_dependency_removed_ids_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

// static
int SharedWorkerServiceImplTest::s_update_worker_dependency_call_count_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_added_ids_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_removed_ids_;

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  scoped_ptr<MockRendererProcessHost> renderer_host(new MockRendererProcessHost(
      100, browser_context_->GetResourceContext(), *partition_.get()));
  scoped_ptr<MockSharedWorkerConnector> connector(
      new MockSharedWorkerConnector(renderer_host.get()));
  int worker_route_id;
  int worker_msg_port_route_id;

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector->Create("http://example.com/w.js", "name", 200, 300);
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

  // SharedWorkerConnector side sends MessagePortHostMsg_QueueMessages in
  // WebSharedWorkerProxy::connect.
  connector->SendQueueMessages();
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // MessagePortMsg_MessagesQueued(2) should be sent back to
  // SharedWorkerConnector side.
  CheckMessagePortMsgMessagesQueued(renderer_host.get(), connector.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(1), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector->SendConnect();
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // WorkerMsg_Connect should be sent to SharedWorker side.
  CheckWorkerMsgConnect(renderer_host.get(),
                        worker_route_id,
                        connector->remote_port_id(),
                        &worker_msg_port_route_id);

  // When SharedWorkerConnector receives MessagePortMsg_MessagesQueued(2), it
  // sends MessagePortHostMsg_SendQueuedMessages.
  std::vector<QueuedMessage> empty_messages;
  connector->SendSendQueuedMessages(empty_messages);
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
          connector->remote_port_id(), worker_route_id)));
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host.get(), connector.get());

  // When SharedWorkerConnector side sends MessagePortHostMsg_PostMessage,
  // SharedWorker side shuold receive MessagePortMsg_Message.
  connector->SendPostMessage("test1");
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host.get(), worker_msg_port_route_id, "test1");

  // When SharedWorker side sends MessagePortHostMsg_PostMessage,
  // SharedWorkerConnector side shuold receive MessagePortMsg_Message.
  const std::vector<int> empty_ids;
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new MessagePortHostMsg_PostMessage(connector->remote_port_id(),
                                         base::ASCIIToUTF16("test2"),
                                         empty_ids)));
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host.get(), connector->local_port_route_id(), "test2");

  // UpdateWorkerDependency should not be called.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  scoped_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(
          100, browser_context_->GetResourceContext(), *partition_.get()));
  scoped_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  int worker_route_id;
  int worker_msg_port_route_id1;

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector1->Create("http://example.com/w.js", "name", 200, 300);
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(renderer_host1.get(),
                                    "http://example.com/w.js",
                                    "name",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  // ViewMsg_WorkerCreated(1) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  // SharedWorkerConnector side sends MessagePortHostMsg_QueueMessages in
  // WebSharedWorkerProxy::connect.
  connector1->SendQueueMessages();
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // MessagePortMsg_MessagesQueued(2) should be sent back to
  // SharedWorkerConnector side.
  CheckMessagePortMsgMessagesQueued(renderer_host1.get(), connector1.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(1), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector1->SendConnect();
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // WorkerMsg_Connect should be sent to SharedWorker side.
  CheckWorkerMsgConnect(renderer_host1.get(),
                        worker_route_id,
                        connector1->remote_port_id(),
                        &worker_msg_port_route_id1);

  // When SharedWorkerConnector receives MessagePortMsg_MessagesQueued(2), it
  // sends MessagePortHostMsg_SendQueuedMessages.
  std::vector<QueuedMessage> empty_messages;
  connector1->SendSendQueuedMessages(empty_messages);
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerScriptLoaded in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(renderer_host1->OnMessageReceived(
      new WorkerHostMsg_WorkerScriptLoaded(worker_route_id)));
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(
      renderer_host1->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          connector1->remote_port_id(), worker_route_id)));
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host1.get(), connector1.get());

  // When SharedWorkerConnector side sends MessagePortHostMsg_PostMessage,
  // SharedWorker side shuold receive MessagePortMsg_Message.
  connector1->SendPostMessage("test1");
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host1.get(), worker_msg_port_route_id1, "test1");

  // When SharedWorker side sends MessagePortHostMsg_PostMessage,
  // SharedWorkerConnector side shuold receive MessagePortMsg_Message.
  const std::vector<int> empty_ids;
  EXPECT_TRUE(renderer_host1->OnMessageReceived(
      new MessagePortHostMsg_PostMessage(connector1->remote_port_id(),
                                         base::ASCIIToUTF16("test2"),
                                         empty_ids)));
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host1.get(), connector1->local_port_route_id(), "test2");

  // The second renderer host.
  scoped_ptr<MockRendererProcessHost> renderer_host2(
      new MockRendererProcessHost(
          400, browser_context_->GetResourceContext(), *partition_.get()));
  scoped_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));
  int worker_msg_port_route_id2;

  // UpdateWorkerDependency should not be called yet.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector2->Create("http://example.com/w.js", "name", 500, 600);
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  // ViewMsg_WorkerCreated(3) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host2.get(), connector2.get());

  // UpdateWorkerDependency should be called.
  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  EXPECT_EQ(1U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(100, s_worker_dependency_added_ids_[0]);
  EXPECT_EQ(0U, s_worker_dependency_removed_ids_.size());

  // SharedWorkerConnector side sends MessagePortHostMsg_QueueMessages in
  // WebSharedWorkerProxy::connect.
  connector2->SendQueueMessages();
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  // MessagePortMsg_MessagesQueued(4) should be sent back to
  // SharedWorkerConnector side.
  CheckMessagePortMsgMessagesQueued(renderer_host2.get(), connector2.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(3), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector2->SendConnect();
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // WorkerMsg_Connect should be sent to SharedWorker side.
  CheckWorkerMsgConnect(renderer_host1.get(),
                        worker_route_id,
                        connector2->remote_port_id(),
                        &worker_msg_port_route_id2);

  // When SharedWorkerConnector receives MessagePortMsg_MessagesQueued(4), it
  // sends MessagePortHostMsg_SendQueuedMessages.
  connector2->SendSendQueuedMessages(empty_messages);
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::OnConnect().
  EXPECT_TRUE(
      renderer_host1->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          connector2->remote_port_id(), worker_route_id)));
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host2.get(), connector2.get());

  // When SharedWorkerConnector side sends MessagePortHostMsg_PostMessage,
  // SharedWorker side shuold receive MessagePortMsg_Message.
  connector2->SendPostMessage("test3");
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host1.get(), worker_msg_port_route_id2, "test3");

  // When SharedWorker side sends MessagePortHostMsg_PostMessage,
  // SharedWorkerConnector side shuold receive MessagePortMsg_Message.
  EXPECT_TRUE(renderer_host1->OnMessageReceived(
      new MessagePortHostMsg_PostMessage(connector2->remote_port_id(),
                                         base::ASCIIToUTF16("test4"),
                                         empty_ids)));
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host2.get(), connector2->local_port_route_id(), "test4");

  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  renderer_host2.reset();
  // UpdateWorkerDependency should be called.
  EXPECT_EQ(2, s_update_worker_dependency_call_count_);
  EXPECT_EQ(0U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(1U, s_worker_dependency_removed_ids_.size());
  EXPECT_EQ(100, s_worker_dependency_removed_ids_[0]);
}

}  // namespace content
