// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_url_loader_factory.h"

#include <stddef.h>
#include <utility>
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_loader.mojom.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/blob/mojo_blob_reader.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

namespace {
constexpr size_t kDefaultAllocationSize = 512 * 1024;

// This class handles a request for a blob:// url. It self-destructs (directly,
// or after passing ownership to storage::MojoBlobReader at the end of the Start
// method) when it has finished responding.
// Note: some of this code is duplicated from storage::BlobURLRequestJob.
class BlobURLLoader : public storage::MojoBlobReader::Delegate,
                      public mojom::URLLoader {
 public:
  BlobURLLoader(mojom::URLLoaderRequest url_loader_request,
                const ResourceRequest& request,
                mojom::URLLoaderClientPtr client,
                std::unique_ptr<storage::BlobDataHandle> blob_handle,
                storage::FileSystemContext* file_system_context)
      : binding_(this, std::move(url_loader_request)),
        client_(std::move(client)),
        blob_handle_(std::move(blob_handle)),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // PostTask since it might destruct.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&BlobURLLoader::Start, weak_factory_.GetWeakPtr(),
                       request, base::WrapRefCounted(file_system_context)));
  }

 private:
  void Start(const ResourceRequest& request,
             scoped_refptr<storage::FileSystemContext> file_system_context) {
    if (!blob_handle_) {
      OnComplete(net::ERR_FILE_NOT_FOUND, 0);
      delete this;
      return;
    }

    // We only support GET request per the spec.
    if (request.method != "GET") {
      OnComplete(net::ERR_METHOD_NOT_SUPPORTED, 0);
      delete this;
      return;
    }

    std::string range_header;
    if (request.headers.GetHeader(net::HttpRequestHeaders::kRange,
                                  &range_header)) {
      // We only care about "Range" header here.
      std::vector<net::HttpByteRange> ranges;
      if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
        if (ranges.size() == 1) {
          byte_range_set_ = true;
          byte_range_ = ranges[0];
        } else {
          // We don't support multiple range requests in one single URL request,
          // because we need to do multipart encoding here.
          // TODO(jianli): Support multipart byte range requests.
          OnComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, 0);
          delete this;
          return;
        }
      }
    }

    storage::MojoBlobReader::Create(file_system_context.get(),
                                    blob_handle_.get(), byte_range_,
                                    base::WrapUnique(this));
  }

  // mojom::URLLoader implementation:
  void FollowRedirect() override { NOTREACHED(); }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  // storage::MojoBlobReader::Delegate implementation:
  mojo::ScopedDataPipeProducerHandle PassDataPipe() override {
    mojo::DataPipe data_pipe(kDefaultAllocationSize);
    response_body_consumer_handle_ = std::move(data_pipe.consumer_handle);
    return std::move(data_pipe.producer_handle);
  }

  RequestSideData DidCalculateSize(uint64_t total_size,
                                   uint64_t content_size) override {
    total_size_ = total_size;
    bool result = byte_range_.ComputeBounds(total_size);
    DCHECK(result);

    net::HttpStatusCode status_code = net::HTTP_OK;
    if (byte_range_set_ && byte_range_.IsValid()) {
      status_code = net::HTTP_PARTIAL_CONTENT;
    } else {
      DCHECK_EQ(total_size, content_size);
      // TODO(horo): When the requester doesn't need the side data
      // (ex:FileReader) we should skip reading the side data.
      return REQUEST_SIDE_DATA;
    }

    HeadersCompleted(status_code, content_size, nullptr);
    return DONT_REQUEST_SIDE_DATA;
  }

  void DidReadSideData(net::IOBufferWithSize* data) override {
    HeadersCompleted(net::HTTP_OK, total_size_, data);
  }

  void HeadersCompleted(net::HttpStatusCode status_code,
                        uint64_t content_size,
                        net::IOBufferWithSize* metadata) {
    ResourceResponseHead response;
    response.content_length = 0;
    if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT)
      response.content_length = content_size;
    response.headers = storage::BlobURLRequestJob::GenerateHeaders(
        status_code, blob_handle_.get(), &byte_range_, total_size_,
        content_size);

    std::string mime_type;
    response.headers->GetMimeType(&mime_type);
    // Match logic in StreamURLRequestJob::HeadersCompleted.
    if (mime_type.empty())
      mime_type = "text/plain";
    response.mime_type = mime_type;

    // TODO(jam): some of this code can be shared with
    // content/network/url_loader_impl.h
    client_->OnReceiveResponse(response, base::nullopt, nullptr);
    sent_headers_ = true;

    if (metadata) {
      const uint8_t* data = reinterpret_cast<const uint8_t*>(metadata->data());
      client_->OnReceiveCachedMetadata(
          std::vector<uint8_t>(data, data + metadata->size()));
    }
  }

  void DidRead(int num_bytes) override {
    if (response_body_consumer_handle_.is_valid()) {
      // Send the data pipe on the first OnReadCompleted call.
      client_->OnStartLoadingResponseBody(
          std::move(response_body_consumer_handle_));
    }
  }

  void OnComplete(net::Error error_code,
                  uint64_t total_written_bytes) override {
    if (error_code != net::OK && !sent_headers_) {
      net::HttpStatusCode status_code =
          storage::BlobURLRequestJob::NetErrorToHttpStatusCode(error_code);
      ResourceResponseHead response;
      response.headers = storage::BlobURLRequestJob::GenerateHeaders(
          status_code, nullptr, nullptr, 0, 0);
      client_->OnReceiveResponse(response, base::nullopt, nullptr);
    }
    ResourceRequestCompletionStatus request_complete_data;
    // TODO(kinuko): We should probably set the error_code here,
    // while it makes existing tests fail. crbug.com/732750
    request_complete_data.completion_time = base::TimeTicks::Now();
    request_complete_data.encoded_body_length = total_written_bytes;
    request_complete_data.decoded_body_length = total_written_bytes;
    client_->OnComplete(request_complete_data);
  }

  mojo::Binding<mojom::URLLoader> binding_;
  mojom::URLLoaderClientPtr client_;

  bool byte_range_set_ = false;
  net::HttpByteRange byte_range_;

  uint64_t total_size_ = 0;
  bool sent_headers_ = false;

  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer_handle_;

  base::WeakPtrFactory<BlobURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLLoader);
};

}  // namespace

// static
scoped_refptr<BlobURLLoaderFactory> BlobURLLoaderFactory::Create(
    BlobContextGetter blob_storage_context_getter,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto factory = base::MakeRefCounted<BlobURLLoaderFactory>(
      std::move(file_system_context));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BlobURLLoaderFactory::InitializeOnIO, factory,
                     std::move(blob_storage_context_getter)));
  return factory;
}

void BlobURLLoaderFactory::HandleRequest(
    mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&BlobURLLoaderFactory::BindOnIO, this,
                                         std::move(request)));
}

BlobURLLoaderFactory::BlobURLLoaderFactory(
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : file_system_context_(std::move(file_system_context)) {}

BlobURLLoaderFactory::~BlobURLLoaderFactory() {}

void BlobURLLoaderFactory::InitializeOnIO(
    BlobContextGetter blob_storage_context_getter) {
  blob_storage_context_ = std::move(blob_storage_context_getter).Run();
}

void BlobURLLoaderFactory::BindOnIO(mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  loader_factory_bindings_.AddBinding(this, std::move(request));
}

// static
void BlobURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest loader,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    std::unique_ptr<storage::BlobDataHandle> blob_handle,
    storage::FileSystemContext* file_system_context) {
  new BlobURLLoader(std::move(loader), request, std::move(client),
                    std::move(blob_handle), file_system_context);
}

void BlobURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<storage::BlobDataHandle> blob_handle;
  if (blob_storage_context_) {
    blob_handle = blob_storage_context_->GetBlobDataFromPublicURL(request.url);
  }
  CreateLoaderAndStart(std::move(loader), request, std::move(client),
                       std::move(blob_handle), file_system_context_.get());
}

void BlobURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  loader_factory_bindings_.AddBinding(this, std::move(request));
}

}  // namespace content
