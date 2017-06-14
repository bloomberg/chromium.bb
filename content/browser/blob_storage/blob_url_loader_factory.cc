// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_url_loader_factory.h"

#include <stddef.h>
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/net_adapters.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/browser_thread.h"
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
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

namespace {
constexpr size_t kDefaultAllocationSize = 512 * 1024;

// This class handles a request for a blob:// url. It self-destructs (see
// DeleteIfNeeded) when it has finished responding.
// Note: some of this code is duplicated from storage::BlobURLRequestJob.
class BlobURLLoader : public mojom::URLLoader {
 public:
  BlobURLLoader(mojom::URLLoaderAssociatedRequest url_loader_request,
                const ResourceRequest& request,
                mojom::URLLoaderClientPtr client,
                std::unique_ptr<storage::BlobDataHandle> blob_handle,
                storage::FileSystemContext* file_system_context)
      : binding_(this, std::move(url_loader_request)),
        client_(std::move(client)),
        blob_handle_(std::move(blob_handle)),
        writable_handle_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        peer_closed_handle_watcher_(FROM_HERE,
                                    mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // PostTask since it might destruct.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BlobURLLoader::Start, weak_factory_.GetWeakPtr(), request,
                   make_scoped_refptr(file_system_context)));
  }

  void Start(const ResourceRequest& request,
             scoped_refptr<storage::FileSystemContext> file_system_context) {
    if (!blob_handle_) {
      NotifyCompleted(net::ERR_FILE_NOT_FOUND);
      return;
    }

    base::SequencedTaskRunner* file_task_runner =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE).get();
    blob_reader_ =
        blob_handle_->CreateReader(file_system_context.get(), file_task_runner);

    // We only support GET request per the spec.
    if (request.method != "GET") {
      NotifyCompleted(net::ERR_METHOD_NOT_SUPPORTED);
      return;
    }

    if (blob_reader_->net_error()) {
      NotifyCompleted(blob_reader_->net_error());
      return;
    }

    net::HttpRequestHeaders request_headers;
    request_headers.AddHeadersFromString(request.headers);
    std::string range_header;
    if (request_headers.GetHeader(net::HttpRequestHeaders::kRange,
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
          NotifyCompleted(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
        }
      }
    }

    storage::BlobReader::Status size_status =
        blob_reader_->CalculateSize(base::Bind(&BlobURLLoader::DidCalculateSize,
                                               weak_factory_.GetWeakPtr()));
    switch (size_status) {
      case storage::BlobReader::Status::NET_ERROR:
        NotifyCompleted(blob_reader_->net_error());
        return;
      case storage::BlobReader::Status::IO_PENDING:
        return;
      case storage::BlobReader::Status::DONE:
        DidCalculateSize(net::OK);
        return;
    }

    NOTREACHED();
  }

  ~BlobURLLoader() override {}

 private:
  // mojom::URLLoader implementation:
  void FollowRedirect() override { NOTREACHED(); }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

  // Notifies the client that the request completed. Takes care of deleting this
  // object now if possible (i.e. no outstanding data pipe), otherwise this
  // object will be deleted when the data pipe is closed.
  void NotifyCompleted(int error_code) {
    if (error_code != net::OK && !sent_headers_) {
      net::HttpStatusCode status_code =
          storage::BlobURLRequestJob::NetErrorToHttpStatusCode(error_code);
      ResourceResponseHead response;
      response.headers = storage::BlobURLRequestJob::GenerateHeaders(
          status_code, nullptr, nullptr, nullptr, nullptr);
      client_->OnReceiveResponse(response, base::nullopt, nullptr);
    }
    ResourceRequestCompletionStatus request_complete_data;
    // TODO(kinuko): We should probably set the error_code here,
    // while it makes existing tests fail. crbug.com/732750
    request_complete_data.completion_time = base::TimeTicks::Now();
    request_complete_data.encoded_body_length = total_written_bytes_;
    request_complete_data.decoded_body_length = total_written_bytes_;
    client_->OnComplete(request_complete_data);

    DeleteIfNeeded();
  }

  void DidCalculateSize(int result) {
    if (result != net::OK) {
      NotifyCompleted(result);
      return;
    }

    // Apply the range requirement.
    if (!byte_range_.ComputeBounds(blob_reader_->total_size())) {
      NotifyCompleted(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return;
    }

    DCHECK_LE(byte_range_.first_byte_position(),
              byte_range_.last_byte_position() + 1);
    uint64_t length =
        base::checked_cast<uint64_t>(byte_range_.last_byte_position() -
                                     byte_range_.first_byte_position() + 1);

    if (byte_range_set_)
      blob_reader_->SetReadRange(byte_range_.first_byte_position(), length);

    net::HttpStatusCode status_code = net::HTTP_OK;
    if (byte_range_set_ && byte_range_.IsValid()) {
      status_code = net::HTTP_PARTIAL_CONTENT;
    } else {
      // TODO(horo): When the requester doesn't need the side data
      // (ex:FileReader) we should skip reading the side data.
      if (blob_reader_->has_side_data() &&
          blob_reader_->ReadSideData(base::Bind(&BlobURLLoader::DidReadMetadata,
                                                weak_factory_.GetWeakPtr())) ==
              storage::BlobReader::Status::IO_PENDING) {
        return;
      }
    }

    HeadersCompleted(status_code);
  }

  void DidReadMetadata(storage::BlobReader::Status result) {
    if (result != storage::BlobReader::Status::DONE) {
      NotifyCompleted(blob_reader_->net_error());
      return;
    }
    HeadersCompleted(net::HTTP_OK);
  }

  void HeadersCompleted(net::HttpStatusCode status_code) {
    ResourceResponseHead response;
    response.content_length = 0;
    response.headers = storage::BlobURLRequestJob::GenerateHeaders(
        status_code, blob_handle_.get(), blob_reader_.get(), &byte_range_,
        &response.content_length);

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

    net::IOBufferWithSize* metadata = blob_reader_->side_data();
    if (metadata) {
      const uint8_t* data = reinterpret_cast<const uint8_t*>(metadata->data());
      client_->OnReceiveCachedMetadata(
          std::vector<uint8_t>(data, data + metadata->size()));
    }

    mojo::DataPipe data_pipe(kDefaultAllocationSize);
    response_body_stream_ = std::move(data_pipe.producer_handle);
    response_body_consumer_handle_ = std::move(data_pipe.consumer_handle);
    peer_closed_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::Bind(&BlobURLLoader::OnResponseBodyStreamClosed,
                   base::Unretained(this)));
    peer_closed_handle_watcher_.ArmOrNotify();

    writable_handle_watcher_.Watch(
        response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::Bind(&BlobURLLoader::OnResponseBodyStreamReady,
                   base::Unretained(this)));

    // Start reading...
    ReadMore();
  }

  void ReadMore() {
    DCHECK(!pending_write_.get());

    uint32_t num_bytes;
    // TODO: we should use the abstractions in MojoAsyncResourceHandler.
    MojoResult result = NetToMojoPendingBuffer::BeginWrite(
        &response_body_stream_, &pending_write_, &num_bytes);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // The pipe is full. We need to wait for it to have more space.
      writable_handle_watcher_.ArmOrNotify();
      return;
    } else if (result != MOJO_RESULT_OK) {
      // The response body stream is in a bad state. Bail.
      writable_handle_watcher_.Cancel();
      response_body_stream_.reset();
      NotifyCompleted(net::ERR_UNEXPECTED);
      return;
    }

    CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);
    scoped_refptr<net::IOBuffer> buf(
        new NetToMojoIOBuffer(pending_write_.get()));
    int bytes_read;
    storage::BlobReader::Status read_status = blob_reader_->Read(
        buf.get(), static_cast<int>(num_bytes), &bytes_read,
        base::Bind(&BlobURLLoader::DidRead, weak_factory_.GetWeakPtr(), false));
    switch (read_status) {
      case storage::BlobReader::Status::NET_ERROR:
        NotifyCompleted(blob_reader_->net_error());
        return;
      case storage::BlobReader::Status::IO_PENDING:
        // Wait for DidRead.
        return;
      case storage::BlobReader::Status::DONE:
        if (bytes_read > 0) {
          DidRead(true, bytes_read);
        } else {
          writable_handle_watcher_.Cancel();
          pending_write_->Complete(0);
          pending_write_ = nullptr;  // This closes the data pipe.
          NotifyCompleted(net::OK);
          return;
        }
    }
  }

  void DidRead(bool completed_synchronously, int num_bytes) {
    if (response_body_consumer_handle_.is_valid()) {
      // Send the data pipe on the first OnReadCompleted call.
      client_->OnStartLoadingResponseBody(
          std::move(response_body_consumer_handle_));
    }
    response_body_stream_ = pending_write_->Complete(num_bytes);
    total_written_bytes_ += num_bytes;
    pending_write_ = nullptr;
    if (completed_synchronously) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&BlobURLLoader::ReadMore, weak_factory_.GetWeakPtr()));
    } else {
      ReadMore();
    }
  }

  void OnResponseBodyStreamClosed(MojoResult result) {
    response_body_stream_.reset();
    pending_write_ = nullptr;
    DeleteIfNeeded();
  }

  void OnResponseBodyStreamReady(MojoResult result) {
    // TODO: Handle a bad |result| value.
    DCHECK_EQ(result, MOJO_RESULT_OK);
    ReadMore();
  }

  void DeleteIfNeeded() {
    bool has_data_pipe =
        pending_write_.get() || response_body_stream_.is_valid();
    if (!has_data_pipe)
      delete this;
  }

  mojo::AssociatedBinding<mojom::URLLoader> binding_;
  mojom::URLLoaderClientPtr client_;

  bool byte_range_set_ = false;
  net::HttpByteRange byte_range_;

  bool sent_headers_ = false;

  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  std::unique_ptr<storage::BlobReader> blob_reader_;

  // TODO(jam): share with URLLoaderImpl
  mojo::ScopedDataPipeProducerHandle response_body_stream_;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer_handle_;
  scoped_refptr<NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher writable_handle_watcher_;
  mojo::SimpleWatcher peer_closed_handle_watcher_;
  int64_t total_written_bytes_ = 0;

  base::WeakPtrFactory<BlobURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLLoader);
};

}  // namespace

BlobURLLoaderFactory::BlobURLLoaderFactory(
    BlobContextGetter blob_storage_context_getter,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : file_system_context_(file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BlobURLLoaderFactory::InitializeOnIO, this,
                     std::move(blob_storage_context_getter)));
}

void BlobURLLoaderFactory::HandleRequest(
    mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&BlobURLLoaderFactory::BindOnIO, this,
                                         std::move(request)));
}

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
    mojom::URLLoaderAssociatedRequest loader,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    std::unique_ptr<storage::BlobDataHandle> blob_handle,
    storage::FileSystemContext* file_system_context) {
  new BlobURLLoader(std::move(loader), request, std::move(client),
                    std::move(blob_handle), file_system_context);
}

void BlobURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderAssociatedRequest loader,
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

void BlobURLLoaderFactory::SyncLoad(int32_t routing_id,
                                    int32_t request_id,
                                    const ResourceRequest& request,
                                    SyncLoadCallback callback) {
  NOTREACHED();
}

}  // namespace content
