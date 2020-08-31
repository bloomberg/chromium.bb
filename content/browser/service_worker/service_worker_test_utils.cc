// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_test_utils.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_messages.mojom.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_info.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"

namespace content {

namespace {

// A mock SharedURLLoaderFactory that always fails to start.
// TODO(bashi): Make this factory not to fail when unit tests actually need
// this to be working.
class MockSharedURLLoaderFactory final
    : public network::SharedURLLoaderFactory {
 public:
  MockSharedURLLoaderFactory() = default;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_NOT_IMPLEMENTED));
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED();
  }

  // network::SharedURLLoaderFactory:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class base::RefCounted<MockSharedURLLoaderFactory>;

  ~MockSharedURLLoaderFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(MockSharedURLLoaderFactory);
};

// Returns MockSharedURLLoaderFactory.
class MockPendingSharedURLLoaderFactory final
    : public network::PendingSharedURLLoaderFactory {
 public:
  MockPendingSharedURLLoaderFactory() = default;
  ~MockPendingSharedURLLoaderFactory() override = default;

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return base::MakeRefCounted<MockSharedURLLoaderFactory>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPendingSharedURLLoaderFactory);
};

class FakeNavigationClient : public mojom::NavigationClient {
 public:
  using ReceivedProviderInfoCallback = base::OnceCallback<void(
      blink::mojom::ServiceWorkerProviderInfoForClientPtr)>;

  explicit FakeNavigationClient(
      ReceivedProviderInfoCallback on_received_callback)
      : on_received_callback_(std::move(on_received_callback)) {}
  ~FakeNavigationClient() override = default;

 private:
  // mojom::NavigationClient implementation:
  void CommitNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token,
      CommitNavigationCallback callback) override {
    std::move(on_received_callback_).Run(std::move(provider_info));
    std::move(callback).Run(nullptr, nullptr);
  }
  void CommitFailedNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
      CommitFailedNavigationCallback callback) override {
    std::move(callback).Run(nullptr, nullptr);
  }

  ReceivedProviderInfoCallback on_received_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationClient);
};

void OnWriteMetadataToDiskCache(
    std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer,
    const GURL& script_url,
    int body_size,
    int meta_data_size,
    WriteToDiskCacheCallback callback,
    int result) {
  EXPECT_EQ(result, meta_data_size);
  std::move(callback).Run(storage::mojom::ServiceWorkerResourceRecord::New(
      metadata_writer->response_id(), script_url, body_size));
}

void OnWriteBodyDataToDiskCache(
    std::unique_ptr<ServiceWorkerResponseWriter> writer,
    std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer,
    const GURL& script_url,
    int body_size,
    const std::string& meta_data,
    WriteToDiskCacheCallback callback,
    int result) {
  EXPECT_EQ(result, body_size);
  scoped_refptr<net::IOBuffer> meta_data_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(meta_data);
  ServiceWorkerResponseMetadataWriter* metadata_writer_rawptr =
      metadata_writer.get();
  metadata_writer_rawptr->WriteMetadata(
      meta_data_buffer.get(), meta_data.size(),
      base::BindOnce(&OnWriteMetadataToDiskCache, std::move(metadata_writer),
                     script_url, body_size, meta_data.size(),
                     std::move(callback)));
}

void OnWriteBodyInfoToDiskCache(
    std::unique_ptr<ServiceWorkerResponseWriter> writer,
    std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer,
    const GURL& script_url,
    const std::string& body,
    const std::string& meta_data,
    WriteToDiskCacheCallback callback,
    int result) {
  EXPECT_GE(result, 0);
  scoped_refptr<net::IOBuffer> body_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(body);
  ServiceWorkerResponseWriter* writer_rawptr = writer.get();
  writer_rawptr->WriteData(
      body_buffer.get(), body.size(),
      base::BindOnce(&OnWriteBodyDataToDiskCache, std::move(writer),
                     std::move(metadata_writer), script_url, body.size(),
                     meta_data, std::move(callback)));
}

void WriteToDiskCacheAsyncInternal(
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data,
    std::unique_ptr<ServiceWorkerResponseWriter> body_writer,
    std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer,
    WriteToDiskCacheCallback callback) {
  std::unique_ptr<net::HttpResponseInfo> http_info =
      std::make_unique<net::HttpResponseInfo>();
  http_info->request_time = base::Time::Now();
  http_info->response_time = base::Time::Now();
  http_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.0 200 OK\0\0");
  for (const auto& header : headers)
    http_info->headers->AddHeader(header.first, header.second);

  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(http_info));
  info_buffer->response_data_size = body.size();
  ServiceWorkerResponseWriter* writer_rawptr = body_writer.get();
  writer_rawptr->WriteInfo(
      info_buffer.get(),
      base::BindOnce(&OnWriteBodyInfoToDiskCache, std::move(body_writer),
                     std::move(metadata_writer), script_url, body, meta_data,
                     std::move(callback)));
}

storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheSyncInternal(
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data,
    std::unique_ptr<ServiceWorkerResponseWriter> body_writer,
    std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer) {
  storage::mojom::ServiceWorkerResourceRecordPtr record;

  base::RunLoop loop;
  WriteToDiskCacheAsyncInternal(
      script_url, headers, body, meta_data, std::move(body_writer),
      std::move(metadata_writer),
      base::BindLambdaForTesting(
          [&](storage::mojom::ServiceWorkerResourceRecordPtr result) {
            record = std::move(result);
            loop.Quit();
          }));
  loop.Run();

  return record;
}

}  // namespace

ServiceWorkerRemoteProviderEndpoint::ServiceWorkerRemoteProviderEndpoint() {}
ServiceWorkerRemoteProviderEndpoint::ServiceWorkerRemoteProviderEndpoint(
    ServiceWorkerRemoteProviderEndpoint&& other)
    : navigation_client_(std::move(other.navigation_client_)),
      host_remote_(std::move(other.host_remote_)),
      client_receiver_(std::move(other.client_receiver_)) {}

ServiceWorkerRemoteProviderEndpoint::~ServiceWorkerRemoteProviderEndpoint() {}

void ServiceWorkerRemoteProviderEndpoint::BindForWindow(
    blink::mojom::ServiceWorkerProviderInfoForClientPtr info) {
  // We establish a message pipe for connecting |navigation_client_| to a fake
  // navigation client, then simulate sending the navigation commit IPC which
  // carries a service worker provider info over it, then the provider info
  // received there gets its |host_remote| and |client_receiver| associated
  // with a message pipe so that their users later can make Mojo calls without
  // crash.
  blink::mojom::ServiceWorkerProviderInfoForClientPtr received_info;
  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeNavigationClient>(base::BindOnce(
          [](base::OnceClosure quit_closure,
             blink::mojom::ServiceWorkerProviderInfoForClientPtr* out_info,
             blink::mojom::ServiceWorkerProviderInfoForClientPtr info) {
            *out_info = std::move(info);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(), &received_info)),
      navigation_client_.BindNewPipeAndPassReceiver());
  navigation_client_->CommitNavigation(
      CreateCommonNavigationParams(), CreateCommitNavigationParams(),
      network::mojom::URLResponseHead::New(),
      mojo::ScopedDataPipeConsumerHandle(), nullptr, nullptr, base::nullopt,
      nullptr, std::move(info), mojo::NullRemote(),
      base::UnguessableToken::Create(),
      base::BindOnce(
          [](std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
                 validated_params,
             mojom::DidCommitProvisionalLoadInterfaceParamsPtr
                 interface_params) {}));
  loop.Run();

  client_receiver_ = std::move(received_info->client_receiver);
  host_remote_.Bind(std::move(received_info->host_remote));
}

void ServiceWorkerRemoteProviderEndpoint::BindForServiceWorker(
    blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr info) {
  host_remote_.Bind(std::move(info->host_remote));
}

ServiceWorkerContainerHostAndInfo::ServiceWorkerContainerHostAndInfo(
    base::WeakPtr<ServiceWorkerContainerHost> host,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr info)
    : host(std::move(host)), info(std::move(info)) {}

ServiceWorkerContainerHostAndInfo::~ServiceWorkerContainerHostAndInfo() =
    default;

base::WeakPtr<ServiceWorkerContainerHost> CreateContainerHostForWindow(
    int process_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint) {
  std::unique_ptr<ServiceWorkerContainerHostAndInfo> host_and_info =
      CreateContainerHostAndInfoForWindow(context, is_parent_frame_secure);
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      std::move(host_and_info->host);
  output_endpoint->BindForWindow(std::move(host_and_info->info));

  // Establish a dummy connection to allow sending messages without errors.
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      reporter;
  auto dummy = reporter.InitWithNewPipeAndPassReceiver();

  // In production code this is called from NavigationRequest in the browser
  // process right before navigation commit.
  container_host->OnBeginNavigationCommit(process_id, 1 /* route_id */,
                                          network::CrossOriginEmbedderPolicy(),
                                          std::move(reporter));
  return container_host;
}

std::unique_ptr<ServiceWorkerContainerHostAndInfo>
CreateContainerHostAndInfoForWindow(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool are_ancestors_secure) {
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
      client_remote;
  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver;
  auto info = blink::mojom::ServiceWorkerProviderInfoForClient::New();
  info->client_receiver = client_remote.InitWithNewEndpointAndPassReceiver();
  host_receiver = info->host_remote.InitWithNewEndpointAndPassReceiver();
  return std::make_unique<ServiceWorkerContainerHostAndInfo>(
      context->CreateContainerHostForWindow(
          std::move(host_receiver), are_ancestors_secure,
          std::move(client_remote), FrameTreeNode::kFrameTreeNodeInvalidId),
      std::move(info));
}

base::OnceCallback<void(blink::ServiceWorkerStatusCode)>
ReceiveServiceWorkerStatus(base::Optional<blink::ServiceWorkerStatusCode>* out,
                           base::OnceClosure quit_closure) {
  return base::BindOnce(
      [](base::OnceClosure quit_closure,
         base::Optional<blink::ServiceWorkerStatusCode>* out,
         blink::ServiceWorkerStatusCode result) {
        *out = result;
        std::move(quit_closure).Run();
      },
      std::move(quit_closure), out);
}

blink::ServiceWorkerStatusCode StartServiceWorker(
    ServiceWorkerVersion* version) {
  blink::ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  version->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                       base::BindLambdaForTesting(
                           [&](blink::ServiceWorkerStatusCode result_status) {
                             status = result_status;
                             run_loop.Quit();
                           }));
  run_loop.Run();
  return status;
}

void StopServiceWorker(ServiceWorkerVersion* version) {
  base::RunLoop run_loop;
  version->StopWorker(run_loop.QuitClosure());
  run_loop.Run();
}

std::unique_ptr<ServiceWorkerProviderHost>
CreateProviderHostForServiceWorkerContext(
    int process_id,
    bool is_parent_frame_secure,
    ServiceWorkerVersion* hosted_version,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint) {
  auto provider_info =
      blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
  auto host = std::make_unique<ServiceWorkerProviderHost>(
      provider_info->host_remote.InitWithNewEndpointAndPassReceiver(),
      hosted_version, std::move(context));

  host->CompleteStartWorkerPreparation(
      process_id,
      provider_info->browser_interface_broker.InitWithNewPipeAndPassReceiver());
  output_endpoint->BindForServiceWorker(std::move(provider_info));
  return host;
}

scoped_refptr<ServiceWorkerRegistration> CreateNewServiceWorkerRegistration(
    ServiceWorkerRegistry* registry,
    const blink::mojom::ServiceWorkerRegistrationOptions& options) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  base::RunLoop run_loop;
  registry->CreateNewRegistration(
      options,
      base::BindLambdaForTesting(
          [&](scoped_refptr<ServiceWorkerRegistration> new_registration) {
            registration = std::move(new_registration);
            run_loop.Quit();
          }));
  run_loop.Run();
  DCHECK(registration);
  return registration;
}

scoped_refptr<ServiceWorkerVersion> CreateNewServiceWorkerVersion(
    ServiceWorkerRegistry* registry,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type) {
  scoped_refptr<ServiceWorkerVersion> version;
  base::RunLoop run_loop;
  registry->CreateNewVersion(
      std::move(registration), script_url, script_type,
      base::BindLambdaForTesting(
          [&](scoped_refptr<ServiceWorkerVersion> new_version) {
            version = std::move(new_version);
            run_loop.Quit();
          }));
  run_loop.Run();
  DCHECK(version);
  return version;
}

scoped_refptr<ServiceWorkerRegistration>
CreateServiceWorkerRegistrationAndVersion(ServiceWorkerContextCore* context,
                                          const GURL& scope,
                                          const GURL& script,
                                          int64_t resource_id) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateNewServiceWorkerRegistration(context->registry(), options);
  scoped_refptr<ServiceWorkerVersion> version =
      CreateNewServiceWorkerVersion(context->registry(), registration.get(),
                                    script, blink::mojom::ScriptType::kClassic);
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(
      storage::mojom::ServiceWorkerResourceRecord::New(resource_id, script,
                                                       /*size_bytes=*/100));
  version->script_cache_map()->SetResources(records);
  version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration->SetWaitingVersion(version);
  return registration;
}

storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheWithIdSync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    int64_t resource_id,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data) {
  std::unique_ptr<ServiceWorkerResponseWriter> body_writer =
      storage->CreateResponseWriter(resource_id);
  std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer =
      storage->CreateResponseMetadataWriter(resource_id);
  return WriteToDiskCacheSyncInternal(script_url, headers, body, meta_data,
                                      std::move(body_writer),
                                      std::move(metadata_writer));
}

storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCacheSync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data) {
  std::unique_ptr<ServiceWorkerResponseWriter> body_writer =
      CreateNewResponseWriterSync(storage);
  std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer =
      storage->CreateResponseMetadataWriter(body_writer->response_id());
  return WriteToDiskCacheSyncInternal(script_url, headers, body, meta_data,
                                      std::move(body_writer),
                                      std::move(metadata_writer));
}

void WriteToDiskCacheAsync(
    ServiceWorkerStorage* storage,
    const GURL& script_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body,
    const std::string& meta_data,
    WriteToDiskCacheCallback callback) {
  std::unique_ptr<ServiceWorkerResponseWriter> body_writer =
      CreateNewResponseWriterSync(storage);
  std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer =
      storage->CreateResponseMetadataWriter(body_writer->response_id());
  WriteToDiskCacheAsyncInternal(
      script_url, headers, body, meta_data, std::move(body_writer),
      std::move(metadata_writer), std::move(callback));
}

std::unique_ptr<ServiceWorkerResponseWriter> CreateNewResponseWriterSync(
    ServiceWorkerStorage* storage) {
  base::RunLoop run_loop;
  std::unique_ptr<ServiceWorkerResponseWriter> writer;
  int64_t resource_id = GetNewResourceIdSync(storage);
  return storage->CreateResponseWriter(resource_id);
}

int64_t GetNewResourceIdSync(ServiceWorkerStorage* storage) {
  base::RunLoop run_loop;
  int64_t resource_id;
  storage->GetNewResourceId(
      base::BindLambdaForTesting([&](int64_t new_resource_id) {
        DCHECK_NE(new_resource_id,
                  blink::mojom::kInvalidServiceWorkerResourceId);
        resource_id = new_resource_id;
        run_loop.Quit();
      }));
  run_loop.Run();
  return resource_id;
}

MockServiceWorkerResponseReader::MockServiceWorkerResponseReader()
    : ServiceWorkerResponseReader(/* resource_id=*/0, /*disk_cache=*/nullptr) {}

MockServiceWorkerResponseReader::~MockServiceWorkerResponseReader() {}

void MockServiceWorkerResponseReader::ReadInfo(
    HttpResponseInfoIOBuffer* info_buf,
    net::CompletionOnceCallback callback) {
  // We need to allocate HttpResponseInfo for
  // ServiceWorkerCacheCacheWriterTest.CopyScript_Async to pass.
  // It reads/writes response headers and the current implementation
  // of ServiceWorkerCacheWriter::WriteInfo() requires a valid
  // HttpResponseInfo. This workaround will be gone once we remove
  // HttpResponseInfo dependencies from service worker codebase.
  DCHECK(!info_buf->http_info);
  info_buf->http_info = std::make_unique<net::HttpResponseInfo>();
  info_buf->http_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.0 200 OK\0\0");

  DCHECK(!expected_reads_.empty());
  ExpectedRead expected = expected_reads_.front();
  EXPECT_TRUE(expected.info);
  if (expected.async) {
    pending_info_ = info_buf;
    pending_callback_ = std::move(callback);
  } else {
    expected_reads_.pop();
    info_buf->response_data_size = expected.len;
    std::move(callback).Run(expected.result);
  }
}

void MockServiceWorkerResponseReader::ReadData(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  DCHECK(!expected_reads_.empty());
  ExpectedRead expected = expected_reads_.front();
  EXPECT_FALSE(expected.info);
  EXPECT_LE(static_cast<int>(expected.len), buf_len);
  if (expected.async) {
    pending_callback_ = std::move(callback);
    pending_buffer_ = buf;
    pending_buffer_len_ = static_cast<size_t>(buf_len);
  } else {
    expected_reads_.pop();
    if (expected.len > 0) {
      size_t to_read = std::min(static_cast<size_t>(buf_len), expected.len);
      memcpy(buf->data(), expected.data, to_read);
    }
    std::move(callback).Run(expected.result);
  }
}

void MockServiceWorkerResponseReader::ExpectReadInfo(size_t len,
                                                     bool async,
                                                     int result) {
  expected_reads_.push(ExpectedRead(len, async, result));
}

void MockServiceWorkerResponseReader::ExpectReadInfoOk(size_t len, bool async) {
  expected_reads_.push(ExpectedRead(len, async, len));
}

void MockServiceWorkerResponseReader::ExpectReadData(const char* data,
                                                     size_t len,
                                                     bool async,
                                                     int result) {
  expected_reads_.push(ExpectedRead(data, len, async, result));
}

void MockServiceWorkerResponseReader::ExpectReadDataOk(const std::string& data,
                                                       bool async) {
  expected_reads_.push(
      ExpectedRead(data.data(), data.size(), async, data.size()));
}

void MockServiceWorkerResponseReader::ExpectReadOk(
    const std::vector<std::string>& stored_data,
    const size_t bytes_stored,
    const bool async) {
  ExpectReadInfoOk(bytes_stored, async);
  for (const auto& data : stored_data)
    ExpectReadDataOk(data, async);
}

void MockServiceWorkerResponseReader::CompletePendingRead() {
  DCHECK(!expected_reads_.empty());
  ExpectedRead expected = expected_reads_.front();
  expected_reads_.pop();
  EXPECT_TRUE(expected.async);
  if (expected.info) {
    pending_info_->response_data_size = expected.len;
  } else {
    size_t to_read = std::min(pending_buffer_len_, expected.len);
    if (to_read > 0)
      memcpy(pending_buffer_->data(), expected.data, to_read);
  }
  pending_info_ = nullptr;
  pending_buffer_ = nullptr;
  net::CompletionOnceCallback callback = std::move(pending_callback_);
  pending_callback_.Reset();
  std::move(callback).Run(expected.result);
}

MockServiceWorkerResponseWriter::MockServiceWorkerResponseWriter()
    : ServiceWorkerResponseWriter(/*resource_id=*/0, /*disk_cache=*/nullptr),
      info_written_(0),
      data_written_(0) {}

MockServiceWorkerResponseWriter::~MockServiceWorkerResponseWriter() = default;

void MockServiceWorkerResponseWriter::WriteInfo(
    HttpResponseInfoIOBuffer* info_buf,
    net::CompletionOnceCallback callback) {
  DCHECK(!expected_writes_.empty());
  ExpectedWrite write = expected_writes_.front();
  EXPECT_TRUE(write.is_info);
  if (write.result > 0) {
    EXPECT_EQ(write.length, static_cast<size_t>(info_buf->response_data_size));
    info_written_ += info_buf->response_data_size;
  }
  if (!write.async) {
    expected_writes_.pop();
    std::move(callback).Run(write.result);
  } else {
    pending_callback_ = std::move(callback);
  }
}

void MockServiceWorkerResponseWriter::WriteData(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  DCHECK(!expected_writes_.empty());
  ExpectedWrite write = expected_writes_.front();
  EXPECT_FALSE(write.is_info);
  if (write.result > 0) {
    EXPECT_EQ(write.length, static_cast<size_t>(buf_len));
    data_written_ += buf_len;
  }
  if (!write.async) {
    expected_writes_.pop();
    std::move(callback).Run(write.result);
  } else {
    pending_callback_ = std::move(callback);
  }
}

void MockServiceWorkerResponseWriter::ExpectWriteInfoOk(size_t length,
                                                        bool async) {
  ExpectWriteInfo(length, async, length);
}

void MockServiceWorkerResponseWriter::ExpectWriteDataOk(size_t length,
                                                        bool async) {
  ExpectWriteData(length, async, length);
}

void MockServiceWorkerResponseWriter::ExpectWriteInfo(size_t length,
                                                      bool async,
                                                      int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  ExpectedWrite expected(true, length, async, result);
  expected_writes_.push(expected);
}

void MockServiceWorkerResponseWriter::ExpectWriteData(size_t length,
                                                      bool async,
                                                      int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  ExpectedWrite expected(false, length, async, result);
  expected_writes_.push(expected);
}

void MockServiceWorkerResponseWriter::CompletePendingWrite() {
  DCHECK(!expected_writes_.empty());
  ExpectedWrite write = expected_writes_.front();
  DCHECK(write.async);
  expected_writes_.pop();
  std::move(pending_callback_).Run(write.result);
}

ServiceWorkerUpdateCheckTestUtils::ServiceWorkerUpdateCheckTestUtils() =
    default;
ServiceWorkerUpdateCheckTestUtils::~ServiceWorkerUpdateCheckTestUtils() =
    default;

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerUpdateCheckTestUtils::CreatePausedCacheWriter(
    EmbeddedWorkerTestHelper* worker_test_helper,
    size_t bytes_compared,
    const std::string& new_headers,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
    uint32_t consumed_size,
    int64_t old_resource_id,
    int64_t new_resource_id) {
  auto cache_writer = ServiceWorkerCacheWriter::CreateForComparison(
      worker_test_helper->context()->storage()->CreateResponseReader(
          old_resource_id),
      worker_test_helper->context()->storage()->CreateResponseReader(
          old_resource_id),
      worker_test_helper->context()->storage()->CreateResponseWriter(
          new_resource_id),
      true /* pause_when_not_identical */);
  cache_writer->response_head_to_write_ =
      network::mojom::URLResponseHead::New();
  cache_writer->response_head_to_write_->request_time = base::Time::Now();
  cache_writer->response_head_to_write_->response_time = base::Time::Now();
  cache_writer->response_head_to_write_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(new_headers);
  cache_writer->bytes_compared_ = bytes_compared;
  cache_writer->data_to_write_ = base::MakeRefCounted<net::WrappedIOBuffer>(
      pending_network_buffer ? pending_network_buffer->buffer() : nullptr);
  cache_writer->len_to_write_ = consumed_size;
  cache_writer->bytes_written_ = 0;
  cache_writer->io_pending_ = true;
  cache_writer->state_ = ServiceWorkerCacheWriter::State::STATE_PAUSING;
  return cache_writer;
}

std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
ServiceWorkerUpdateCheckTestUtils::CreateUpdateCheckerPausedState(
    std::unique_ptr<ServiceWorkerCacheWriter> cache_writer,
    ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
    ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state,
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
    uint32_t consumed_size) {
  mojo::PendingRemote<network::mojom::URLLoaderClient> network_loader_client;
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      network_loader_client_receiver =
          network_loader_client.InitWithNewPipeAndPassReceiver();
  return std::make_unique<ServiceWorkerSingleScriptUpdateChecker::PausedState>(
      std::move(cache_writer), /*network_loader=*/nullptr,
      std::move(network_loader_client_receiver),
      std::move(pending_network_buffer), consumed_size, network_loader_state,
      body_writer_state);
}

void ServiceWorkerUpdateCheckTestUtils::SetComparedScriptInfoForVersion(
    const GURL& script_url,
    int64_t resource_id,
    ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state,
    ServiceWorkerVersion* version) {
  std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo> info_map;
  info_map.emplace(script_url,
                   ServiceWorkerUpdateChecker::ComparedScriptInfo(
                       resource_id, compare_result, std::move(paused_state),
                       /*failure_info=*/nullptr));
  version->PrepareForUpdate(
      std::move(info_map),
      (compare_result ==
       ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent)
          ? script_url
          : GURL(),
      network::CrossOriginEmbedderPolicy());
}

void ServiceWorkerUpdateCheckTestUtils::
    CreateAndSetComparedScriptInfoForVersion(
        const GURL& script_url,
        size_t bytes_compared,
        const std::string& new_headers,
        const std::string& diff_data_block,
        int64_t old_resource_id,
        int64_t new_resource_id,
        EmbeddedWorkerTestHelper* worker_test_helper,
        ServiceWorkerUpdatedScriptLoader::LoaderState network_loader_state,
        ServiceWorkerUpdatedScriptLoader::WriterState body_writer_state,
        ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
        ServiceWorkerVersion* version,
        mojo::ScopedDataPipeProducerHandle* out_body_handle) {
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t bytes_available = 0;
  if (!diff_data_block.empty()) {
    mojo::ScopedDataPipeConsumerHandle network_consumer;
    // Create a data pipe which has the new block sent from the network.
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, out_body_handle,
                                                   &network_consumer));
    uint32_t written_size = diff_data_block.size();
    ASSERT_EQ(MOJO_RESULT_OK,
              (*out_body_handle)
                  ->WriteData(diff_data_block.c_str(), &written_size,
                              MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
    ASSERT_EQ(diff_data_block.size(), written_size);
    base::RunLoop().RunUntilIdle();

    // Read the data to make a pending buffer.
    ASSERT_EQ(MOJO_RESULT_OK,
              network::MojoToNetPendingBuffer::BeginRead(
                  &network_consumer, &pending_buffer, &bytes_available));
    ASSERT_EQ(diff_data_block.size(), bytes_available);
  }

  auto cache_writer = CreatePausedCacheWriter(
      worker_test_helper, bytes_compared, new_headers, pending_buffer,
      bytes_available, old_resource_id, new_resource_id);
  auto paused_state = CreateUpdateCheckerPausedState(
      std::move(cache_writer), network_loader_state, body_writer_state,
      pending_buffer, bytes_available);
  SetComparedScriptInfoForVersion(script_url, old_resource_id, compare_result,
                                  std::move(paused_state), version);
}

bool ServiceWorkerUpdateCheckTestUtils::VerifyStoredResponse(
    int64_t resource_id,
    ServiceWorkerStorage* storage,
    const std::string& expected_body) {
  DCHECK(storage);
  if (resource_id == blink::mojom::kInvalidServiceWorkerResourceId)
    return false;

  // Verify the response status.
  size_t response_data_size = 0;
  {
    std::unique_ptr<ServiceWorkerResponseReader> reader =
        storage->CreateResponseReader(resource_id);
    auto info_buffer = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
    net::TestCompletionCallback cb;
    reader->ReadInfo(info_buffer.get(), cb.callback());
    int rv = cb.WaitForResult();
    if (rv < 0)
      return false;
    EXPECT_LT(0, rv);
    EXPECT_EQ("OK", info_buffer->http_info->headers->GetStatusText());
    response_data_size = info_buffer->response_data_size;
  }

  // Verify the response body.
  {
    std::unique_ptr<ServiceWorkerResponseReader> reader =
        storage->CreateResponseReader(resource_id);
    auto buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(response_data_size);
    net::TestCompletionCallback cb;
    reader->ReadData(buffer.get(), buffer->size(), cb.callback());
    int rv = cb.WaitForResult();
    if (rv < 0)
      return false;
    EXPECT_EQ(static_cast<int>(expected_body.size()), rv);

    std::string received_body(buffer->data(), rv);
    EXPECT_EQ(expected_body, received_body);
  }
  return true;
}

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    uint32_t num_bytes;
    const void* buffer = nullptr;
    MojoResult rv =
        handle.BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        EXPECT_NE(nullptr, buffer);
        EXPECT_GT(num_bytes, 0u);
        uint32_t before_size = result->size();
        result->append(static_cast<const char*>(buffer), num_bytes);
        uint32_t read_size = result->size() - before_size;
        EXPECT_EQ(num_bytes, read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(MOJO_RESULT_OK, rv);
        break;
    }
  }
  NOTREACHED();
  return;
}

std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

}  // namespace content
