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
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

using blink::MessagePortChannel;

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

  static void CheckReceivedFactoryRequest(
      mojom::SharedWorkerFactoryRequest* request) {
    ASSERT_FALSE(s_factory_request_received_.empty());
    *request = std::move(s_factory_request_received_.front());
    s_factory_request_received_.pop();
  }

  static void CheckNotReceivedFactoryRequest() {
    EXPECT_TRUE(s_factory_request_received_.empty());
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
        ->ChangeTryReserveWorkerFuncForTesting(
            &SharedWorkerServiceImplTest::MockTryReserveWorker);
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
  static bool MockTryReserveWorker(
      int worker_process_id,
      mojom::SharedWorkerFactoryPtrInfo* factory_info) {
    if (factory_info)
      s_factory_request_received_.push(mojo::MakeRequest(factory_info));
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
  static std::queue<mojom::SharedWorkerFactoryRequest>
      s_factory_request_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

// static
int SharedWorkerServiceImplTest::s_update_worker_dependency_call_count_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_added_ids_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_removed_ids_;
base::Lock SharedWorkerServiceImplTest::s_lock_;
std::set<int> SharedWorkerServiceImplTest::s_running_process_id_set_;
std::queue<mojom::SharedWorkerFactoryRequest>
    SharedWorkerServiceImplTest::s_factory_request_received_;

namespace {

static const int kProcessIDs[] = {100, 101, 102};
static const int kRenderFrameRouteIDs[] = {300, 301, 302};

std::vector<uint8_t> StringPieceToVector(base::StringPiece s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

void BlockingReadFromMessagePort(MessagePortChannel port,
                                 std::vector<uint8_t>* message) {
  base::RunLoop run_loop;
  port.SetCallback(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<MessagePortChannel> should_be_empty;
  EXPECT_TRUE(port.GetMessage(message, &should_be_empty));
  EXPECT_TRUE(should_be_empty.empty());
}

class MockSharedWorkerMessageFilter : public SharedWorkerMessageFilter {
 public:
  MockSharedWorkerMessageFilter(
      int render_process_id,
      ResourceContext* resource_context,
      const WorkerStoragePartition& partition,
      const SharedWorkerMessageFilter::NextRoutingIDCallback& callback)
      : SharedWorkerMessageFilter(render_process_id, callback) {
    OnFilterAdded(nullptr);
  }

  bool Send(IPC::Message* message) override {
    CHECK(false);  // not reached
    return false;
  }

  void Close() {
    OnChannelClosing();
    OnFilterRemoved();
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    CHECK(false);  // not reached
    return false;
  }

 private:
  ~MockSharedWorkerMessageFilter() override {}
};

class MockSharedWorker : public mojom::SharedWorker {
 public:
  explicit MockSharedWorker(mojom::SharedWorkerRequest request)
      : binding_(this, std::move(request)) {}

  void CheckReceivedConnect(int* connection_request_id,
                            MessagePortChannel* port) {
    ASSERT_FALSE(connect_received_.empty());
    if (connection_request_id)
      *connection_request_id = connect_received_.front().first;
    if (port)
      *port = connect_received_.front().second;
    connect_received_.pop();
  }

  void CheckNotReceivedConnect() { EXPECT_TRUE(connect_received_.empty()); }

  void CheckReceivedTerminate() {
    EXPECT_TRUE(terminate_received_);
    terminate_received_ = false;
  }

 private:
  // mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               mojo::ScopedMessagePipeHandle port) override {
    connect_received_.emplace(connection_request_id,
                              MessagePortChannel(std::move(port)));
  }
  void Terminate() override {
    // Allow duplicate events.
    terminate_received_ = true;
  }

  mojo::Binding<mojom::SharedWorker> binding_;
  std::queue<std::pair<int, MessagePortChannel>> connect_received_;
  bool terminate_received_ = false;
};

class MockSharedWorkerFactory : public mojom::SharedWorkerFactory {
 public:
  explicit MockSharedWorkerFactory(mojom::SharedWorkerFactoryRequest request)
      : binding_(this, std::move(request)) {}

  void CheckReceivedCreateSharedWorker(
      const std::string& expected_url,
      const std::string& expected_name,
      blink::WebContentSecurityPolicyType expected_content_security_policy_type,
      mojom::SharedWorkerHostPtr* host,
      mojom::SharedWorkerRequest* request) {
    std::unique_ptr<CreateParams> create_params = std::move(create_params_);
    ASSERT_TRUE(create_params);
    EXPECT_EQ(GURL(expected_url), create_params->info->url);
    EXPECT_EQ(expected_name, create_params->info->name);
    EXPECT_EQ(expected_content_security_policy_type,
              create_params->info->content_security_policy_type);
    *host = std::move(create_params->host);
    *request = std::move(create_params->request);
  }

  void CheckNotReceivedCreateSharedWorker() { EXPECT_FALSE(create_params_); }

 private:
  // mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
      mojom::SharedWorkerInfoPtr info,
      bool pause_on_start,
      int32_t route_id,
      blink::mojom::WorkerContentSettingsProxyPtr content_settings,
      mojom::SharedWorkerHostPtr host,
      mojom::SharedWorkerRequest request) override {
    CHECK(!create_params_);
    create_params_ = std::make_unique<CreateParams>();
    create_params_->info = std::move(info);
    create_params_->pause_on_start = pause_on_start;
    create_params_->route_id = route_id;
    create_params_->content_settings = std::move(content_settings);
    create_params_->host = std::move(host);
    create_params_->request = std::move(request);
  }

  struct CreateParams {
    mojom::SharedWorkerInfoPtr info;
    bool pause_on_start;
    int32_t route_id;
    blink::mojom::WorkerContentSettingsProxyPtr content_settings;
    mojom::SharedWorkerHostPtr host;
    mojom::SharedWorkerRequest request;
  };

  mojo::Binding<mojom::SharedWorkerFactory> binding_;
  std::unique_ptr<CreateParams> create_params_;
};

class MockSharedWorkerClient : public mojom::SharedWorkerClient {
 public:
  MockSharedWorkerClient() : binding_(this) {}

  void Bind(mojom::SharedWorkerClientRequest request) {
    binding_.Bind(std::move(request));
  }

  void Close() { binding_.Close(); }

  void CheckReceivedOnCreated() {
    EXPECT_TRUE(on_created_received_);
    on_created_received_ = false;
  }

  void CheckReceivedOnConnected(
      std::set<blink::mojom::WebFeature> expected_used_features) {
    EXPECT_TRUE(on_connected_received_);
    on_connected_received_ = false;
    EXPECT_EQ(expected_used_features, on_connected_features_);
  }

  void CheckReceivedOnFeatureUsed(blink::mojom::WebFeature feature) {
    EXPECT_TRUE(on_feature_used_received_);
    on_feature_used_received_ = false;
    EXPECT_EQ(feature, on_feature_used_feature_);
  }

  void CheckNotReceivedOnFeatureUsed() {
    EXPECT_FALSE(on_feature_used_received_);
  }

 private:
  // mojom::SharedWorkerClient methods:
  void OnCreated(blink::mojom::SharedWorkerCreationContextType
                     creation_context_type) override {
    CHECK(!on_created_received_);
    on_created_received_ = true;
  }
  void OnConnected(
      const std::vector<blink::mojom::WebFeature>& features_used) override {
    CHECK(!on_connected_received_);
    on_connected_received_ = true;
    for (auto feature : features_used)
      on_connected_features_.insert(feature);
  }
  void OnScriptLoadFailed() override { NOTREACHED(); }
  void OnFeatureUsed(blink::mojom::WebFeature feature) override {
    CHECK(!on_feature_used_received_);
    on_feature_used_received_ = true;
    on_feature_used_feature_ = feature;
  }

  mojo::Binding<mojom::SharedWorkerClient> binding_;
  bool on_created_received_ = false;
  bool on_connected_received_ = false;
  std::set<blink::mojom::WebFeature> on_connected_features_;
  bool on_feature_used_received_ = false;
  blink::mojom::WebFeature on_feature_used_feature_ =
      blink::mojom::WebFeature();
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
                       base::Unretained(&next_routing_id_)))) {
    SharedWorkerServiceImplTest::RegisterRunningProcessID(process_id);
  }

  ~MockRendererProcessHost() {
    SharedWorkerServiceImplTest::UnregisterRunningProcessID(process_id_);
    worker_filter_->Close();
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
  base::AtomicSequenceNumber next_routing_id_;
  scoped_refptr<MockSharedWorkerMessageFilter> worker_filter_;
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
        false /* data_saver_enabled */));

    mojo::MessagePipe message_pipe;
    local_port_ = MessagePortChannel(std::move(message_pipe.handle0));

    mojom::SharedWorkerClientPtr client_proxy;
    client->Bind(mojo::MakeRequest(&client_proxy));

    connector->Connect(std::move(info), std::move(client_proxy),
                       blink::mojom::SharedWorkerCreationContextType::kSecure,
                       std::move(message_pipe.handle1));
  }
  MessagePortChannel local_port() { return local_port_; }

 private:
  mojom::SharedWorkerClientRequest client_request_;
  MockRendererProcessHost* renderer_host_;
  MessagePortChannel local_port_;
};

}  // namespace

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<MockRendererProcessHost> renderer_host(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector(
      new MockSharedWorkerConnector(renderer_host.get()));
  MockSharedWorkerClient client;

  connector->Create("http://example.com/w.js", "name", kRenderFrameRouteIDs[0],
                    &client);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request;
  CheckReceivedFactoryRequest(&factory_request);
  MockSharedWorkerFactory factory(std::move(factory_request));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  factory.CheckReceivedCreateSharedWorker(
      "http://example.com/w.js", "name",
      blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request);
  MockSharedWorker worker(std::move(worker_request));
  RunAllPendingInMessageLoop();

  int connection_request_id;
  MessagePortChannel port;
  worker.CheckReceivedConnect(&connection_request_id, &port);

  client.CheckReceivedOnCreated();

  // Simulate events the shared worker would send.
  worker_host->OnReadyForInspection();
  worker_host->OnScriptLoaded();
  worker_host->OnConnected(connection_request_id);

  RunAllPendingInMessageLoop();
  client.CheckReceivedOnConnected(std::set<blink::mojom::WebFeature>());

  // Verify that |port| corresponds to |connector->local_port()|.
  std::vector<uint8_t> expected_message(StringPieceToVector("test1"));
  connector->local_port().PostMessage(expected_message.data(),
                                      expected_message.size(),
                                      std::vector<MessagePortChannel>());
  std::vector<uint8_t> received_message;
  BlockingReadFromMessagePort(port, &received_message);
  EXPECT_EQ(expected_message, received_message);

  // Send feature from shared worker to host.
  auto feature1 = static_cast<blink::mojom::WebFeature>(124);
  worker_host->OnFeatureUsed(feature1);
  RunAllPendingInMessageLoop();
  client.CheckReceivedOnFeatureUsed(feature1);
  // A message should be sent only one time per feature.
  worker_host->OnFeatureUsed(feature1);
  RunAllPendingInMessageLoop();
  client.CheckNotReceivedOnFeatureUsed();

  // Send another feature.
  auto feature2 = static_cast<blink::mojom::WebFeature>(901);
  worker_host->OnFeatureUsed(feature2);
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

  // Sends ViewHostMsg_CreateWorker.
  connector0->Create("http://example.com/w.js", "name", kRenderFrameRouteIDs[0],
                     &client0);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request;
  CheckReceivedFactoryRequest(&factory_request);
  MockSharedWorkerFactory factory(std::move(factory_request));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  factory.CheckReceivedCreateSharedWorker(
      "http://example.com/w.js", "name",
      blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request);
  MockSharedWorker worker(std::move(worker_request));
  RunAllPendingInMessageLoop();

  int connection_request_id0;
  MessagePortChannel port0;
  worker.CheckReceivedConnect(&connection_request_id0, &port0);

  client0.CheckReceivedOnCreated();

  // Simulate events the shared worker would send.
  worker_host->OnReadyForInspection();
  worker_host->OnScriptLoaded();
  worker_host->OnConnected(connection_request_id0);

  RunAllPendingInMessageLoop();
  client0.CheckReceivedOnConnected(std::set<blink::mojom::WebFeature>());

  // Verify that |port0| corresponds to |connector0->local_port()|.
  std::vector<uint8_t> expected_message0(StringPieceToVector("test1"));
  connector0->local_port().PostMessage(expected_message0.data(),
                                       expected_message0.size(),
                                       std::vector<MessagePortChannel>());
  std::vector<uint8_t> received_message0;
  BlockingReadFromMessagePort(port0, &received_message0);
  EXPECT_EQ(expected_message0, received_message0);

  auto feature1 = static_cast<blink::mojom::WebFeature>(124);
  worker_host->OnFeatureUsed(feature1);
  RunAllPendingInMessageLoop();
  client0.CheckReceivedOnFeatureUsed(feature1);
  auto feature2 = static_cast<blink::mojom::WebFeature>(901);
  worker_host->OnFeatureUsed(feature2);
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

  // Should not have tried to create a new shared worker.
  CheckNotReceivedFactoryRequest();

  int connection_request_id1;
  MessagePortChannel port1;
  worker.CheckReceivedConnect(&connection_request_id1, &port1);

  client1.CheckReceivedOnCreated();

  // UpdateWorkerDependency should be called.
  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  EXPECT_EQ(1U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(kProcessIDs[0], s_worker_dependency_added_ids_[0]);
  EXPECT_EQ(0U, s_worker_dependency_removed_ids_.size());

  worker_host->OnConnected(connection_request_id1);

  RunAllPendingInMessageLoop();
  client1.CheckReceivedOnConnected({feature1, feature2});

  // Verify that |worker_msg_port2| corresponds to |connector1->local_port()|.
  std::vector<uint8_t> expected_message1(StringPieceToVector("test2"));
  connector1->local_port().PostMessage(expected_message1.data(),
                                       expected_message1.size(),
                                       std::vector<MessagePortChannel>());
  std::vector<uint8_t> received_message1;
  BlockingReadFromMessagePort(port1, &received_message1);
  EXPECT_EQ(expected_message1, received_message1);

  worker_host->OnFeatureUsed(feature1);
  RunAllPendingInMessageLoop();
  client0.CheckNotReceivedOnFeatureUsed();
  client1.CheckNotReceivedOnFeatureUsed();

  auto feature3 = static_cast<blink::mojom::WebFeature>(1019);
  worker_host->OnFeatureUsed(feature3);
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

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase) {
  const char kURL[] = "http://example.com/w.js";
  const char kName[] = "name";

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

  // First client, creates worker.

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  connector0->Create(kURL, kName, kRenderFrameRouteIDs[0], &client0);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request;
  CheckReceivedFactoryRequest(&factory_request);
  MockSharedWorkerFactory factory(std::move(factory_request));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  factory.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request);
  MockSharedWorker worker(std::move(worker_request));
  RunAllPendingInMessageLoop();

  worker.CheckReceivedConnect(nullptr, nullptr);
  client0.CheckReceivedOnCreated();

  // Second client, same worker.

  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;
  connector1->Create(kURL, kName, kRenderFrameRouteIDs[1], &client1);
  RunAllPendingInMessageLoop();

  CheckNotReceivedFactoryRequest();

  worker.CheckReceivedConnect(nullptr, nullptr);
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  RunAllPendingInMessageLoop();

  worker.CheckReceivedTerminate();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase_URLMismatch) {
  const char kURL0[] = "http://example.com/w0.js";
  const char kURL1[] = "http://example.com/w1.js";
  const char kName[] = "name";

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

  // First client, creates worker.

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  connector0->Create(kURL0, kName, kRenderFrameRouteIDs[0], &client0);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request0;
  CheckReceivedFactoryRequest(&factory_request0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  factory0.CheckReceivedCreateSharedWorker(
      kURL0, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0);
  MockSharedWorker worker0(std::move(worker_request0));
  RunAllPendingInMessageLoop();

  worker0.CheckReceivedConnect(nullptr, nullptr);
  client0.CheckReceivedOnCreated();

  // Second client, creates worker.

  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;
  connector1->Create(kURL1, kName, kRenderFrameRouteIDs[1], &client1);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request1;
  CheckReceivedFactoryRequest(&factory_request1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  factory1.CheckReceivedCreateSharedWorker(
      kURL1, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1);
  MockSharedWorker worker1(std::move(worker_request1));
  RunAllPendingInMessageLoop();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  RunAllPendingInMessageLoop();

  worker0.CheckReceivedTerminate();
  worker1.CheckReceivedTerminate();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase_NameMismatch) {
  const char kURL[] = "http://example.com/w.js";
  const char kName0[] = "name0";
  const char kName1[] = "name1";

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

  // First client, creates worker.

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  connector0->Create(kURL, kName0, kRenderFrameRouteIDs[0], &client0);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request0;
  CheckReceivedFactoryRequest(&factory_request0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  factory0.CheckReceivedCreateSharedWorker(
      kURL, kName0, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0);
  MockSharedWorker worker0(std::move(worker_request0));
  RunAllPendingInMessageLoop();

  worker0.CheckReceivedConnect(nullptr, nullptr);
  client0.CheckReceivedOnCreated();

  // Second client, creates worker.

  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;
  connector1->Create(kURL, kName1, kRenderFrameRouteIDs[1], &client1);
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerFactoryRequest factory_request1;
  CheckReceivedFactoryRequest(&factory_request1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));
  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  factory1.CheckReceivedCreateSharedWorker(
      kURL, kName1, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1);
  MockSharedWorker worker1(std::move(worker_request1));
  RunAllPendingInMessageLoop();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  RunAllPendingInMessageLoop();

  worker0.CheckReceivedTerminate();
  worker1.CheckReceivedTerminate();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase) {
  const char kURL[] = "http://example.com/w.js";
  const char kName[] = "name";

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

  // First client and second client are created before the worker starts.

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  connector0->Create(kURL, kName, kRenderFrameRouteIDs[0], &client0);

  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;
  connector1->Create(kURL, kName, kRenderFrameRouteIDs[1], &client1);

  RunAllPendingInMessageLoop();

  // Check that the worker was created.

  mojom::SharedWorkerFactoryRequest factory_request;
  CheckReceivedFactoryRequest(&factory_request);
  MockSharedWorkerFactory factory(std::move(factory_request));

  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  factory.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host,
      &worker_request);
  MockSharedWorker worker(std::move(worker_request));

  RunAllPendingInMessageLoop();

  // Check that the worker received two connections.

  worker.CheckReceivedConnect(nullptr, nullptr);
  client0.CheckReceivedOnCreated();

  worker.CheckReceivedConnect(nullptr, nullptr);
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  RunAllPendingInMessageLoop();

  worker.CheckReceivedTerminate();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase_URLMismatch) {
  const char kURL0[] = "http://example.com/w0.js";
  const char kURL1[] = "http://example.com/w1.js";
  const char kName[] = "name";

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

  // First client and second client are created before the workers start.

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  connector0->Create(kURL0, kName, kRenderFrameRouteIDs[0], &client0);

  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;
  connector1->Create(kURL1, kName, kRenderFrameRouteIDs[1], &client1);

  RunAllPendingInMessageLoop();

  // Check that both workers were created.

  mojom::SharedWorkerFactoryRequest factory_request0;
  CheckReceivedFactoryRequest(&factory_request0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  mojom::SharedWorkerFactoryRequest factory_request1;
  CheckReceivedFactoryRequest(&factory_request1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  factory0.CheckReceivedCreateSharedWorker(
      kURL0, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0);
  MockSharedWorker worker0(std::move(worker_request0));

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  factory1.CheckReceivedCreateSharedWorker(
      kURL1, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1);
  MockSharedWorker worker1(std::move(worker_request1));

  RunAllPendingInMessageLoop();

  // Check that the workers each received a connection.

  worker0.CheckReceivedConnect(nullptr, nullptr);
  worker0.CheckNotReceivedConnect();
  client0.CheckReceivedOnCreated();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  worker1.CheckNotReceivedConnect();
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  RunAllPendingInMessageLoop();

  worker0.CheckReceivedTerminate();
  worker1.CheckReceivedTerminate();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase_NameMismatch) {
  const char kURL[] = "http://example.com/w.js";
  const char kName0[] = "name0";
  const char kName1[] = "name1";

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

  // First client and second client are created before the workers start.

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  MockSharedWorkerClient client0;
  connector0->Create(kURL, kName0, kRenderFrameRouteIDs[0], &client0);

  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  MockSharedWorkerClient client1;
  connector1->Create(kURL, kName1, kRenderFrameRouteIDs[1], &client1);

  RunAllPendingInMessageLoop();

  // Check that both workers were created.

  mojom::SharedWorkerFactoryRequest factory_request0;
  CheckReceivedFactoryRequest(&factory_request0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  mojom::SharedWorkerFactoryRequest factory_request1;
  CheckReceivedFactoryRequest(&factory_request1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  factory0.CheckReceivedCreateSharedWorker(
      kURL, kName0, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0);
  MockSharedWorker worker0(std::move(worker_request0));

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  factory1.CheckReceivedCreateSharedWorker(
      kURL, kName1, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1);
  MockSharedWorker worker1(std::move(worker_request1));

  RunAllPendingInMessageLoop();

  // Check that the workers each received a connection.

  worker0.CheckReceivedConnect(nullptr, nullptr);
  worker0.CheckNotReceivedConnect();
  client0.CheckReceivedOnCreated();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  worker1.CheckNotReceivedConnect();
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  RunAllPendingInMessageLoop();

  worker0.CheckReceivedTerminate();
  worker1.CheckReceivedTerminate();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest) {
  const char kURL[] = "http://example.com/w.js";
  const char kName[] = "name";

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

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));

  MockSharedWorkerClient client0;
  connector0->Create(kURL, kName, kRenderFrameRouteIDs[0], &client0);

  RunAllPendingInMessageLoop();

  // Starts a worker.

  mojom::SharedWorkerFactoryRequest factory_request0;
  CheckReceivedFactoryRequest(&factory_request0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host0;
  mojom::SharedWorkerRequest worker_request0;
  factory0.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host0,
      &worker_request0);
  MockSharedWorker worker0(std::move(worker_request0));

  RunAllPendingInMessageLoop();

  worker0.CheckReceivedConnect(nullptr, nullptr);
  client0.CheckReceivedOnCreated();

  // Kill this process, which should make worker0 unavailable.
  renderer_host0->FastShutdownIfPossible();

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  connector1->Create(kURL, kName, kRenderFrameRouteIDs[1], &client1);

  RunAllPendingInMessageLoop();

  // The previous worker is unavailable, so a new worker is created.

  mojom::SharedWorkerFactoryRequest factory_request1;
  CheckReceivedFactoryRequest(&factory_request1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  RunAllPendingInMessageLoop();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  factory1.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1);
  MockSharedWorker worker1(std::move(worker_request1));

  RunAllPendingInMessageLoop();

  worker0.CheckNotReceivedConnect();
  worker1.CheckReceivedConnect(nullptr, nullptr);
  client1.CheckReceivedOnCreated();

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  connector2->Create(kURL, kName, kRenderFrameRouteIDs[2], &client2);

  RunAllPendingInMessageLoop();

  CheckNotReceivedFactoryRequest();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  client2.CheckReceivedOnCreated();
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest2) {
  const char kURL[] = "http://example.com/w.js";
  const char kName[] = "name";

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

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));

  MockSharedWorkerClient client0;
  connector0->Create(kURL, kName, kRenderFrameRouteIDs[0], &client0);

  // Kill this process, which should make worker0 unavailable.
  renderer_host0->FastShutdownIfPossible();

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  connector1->Create(kURL, kName, kRenderFrameRouteIDs[1], &client1);

  RunAllPendingInMessageLoop();

  // The previous worker is unavailable, so a new worker is created.

  mojom::SharedWorkerFactoryRequest factory_request0;
  CheckReceivedFactoryRequest(&factory_request0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  mojom::SharedWorkerFactoryRequest factory_request1;
  CheckReceivedFactoryRequest(&factory_request1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  RunAllPendingInMessageLoop();

  factory0.CheckNotReceivedCreateSharedWorker();

  mojom::SharedWorkerHostPtr worker_host1;
  mojom::SharedWorkerRequest worker_request1;
  factory1.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::kWebContentSecurityPolicyTypeReport, &worker_host1,
      &worker_request1);
  MockSharedWorker worker1(std::move(worker_request1));

  RunAllPendingInMessageLoop();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  client1.CheckReceivedOnCreated();

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  connector2->Create(kURL, kName, kRenderFrameRouteIDs[2], &client2);

  RunAllPendingInMessageLoop();

  CheckNotReceivedFactoryRequest();

  worker1.CheckReceivedConnect(nullptr, nullptr);
  client2.CheckReceivedOnCreated();
}

}  // namespace content
