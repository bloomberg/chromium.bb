// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/url_loader_impl.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/common/loader_util.h"
#include "content/network/network_context.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content {

namespace {
constexpr size_t kDefaultAllocationSize = 512 * 1024;

// TODO: this duplicates ResourceDispatcherHostImpl::BuildLoadFlagsForRequest.
int BuildLoadFlagsForRequest(const ResourceRequest& request,
                             bool is_sync_load) {
  int load_flags = request.load_flags;

  // Although EV status is irrelevant to sub-frames and sub-resources, we have
  // to perform EV certificate verification on all resources because an HTTP
  // keep-alive connection created to load a sub-frame or a sub-resource could
  // be reused to load a main frame.
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;
  } else if (request.resource_type == RESOURCE_TYPE_PREFETCH) {
    load_flags |= net::LOAD_PREFETCH;
  }

  if (is_sync_load)
    load_flags |= net::LOAD_IGNORE_LIMITS;

  return load_flags;
}

// TODO: this duplicates some of PopulateResourceResponse in
// content/browser/loader/resource_loader.cc
void PopulateResourceResponse(net::URLRequest* request,
                              ResourceResponse* response) {
  response->head.request_time = request->request_time();
  response->head.response_time = request->response_time();
  response->head.headers = request->response_headers();
  request->GetCharset(&response->head.charset);
  response->head.content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->head.mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->head.was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->head.was_alpn_negotiated = response_info.was_alpn_negotiated;
  response->head.alpn_negotiated_protocol =
      response_info.alpn_negotiated_protocol;
  response->head.connection_info = response_info.connection_info;
  response->head.socket_address = response_info.socket_address;

  response->head.effective_connection_type =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  request->GetLoadTimingInfo(&response->head.load_timing);

  response->head.request_start = request->creation_time();
  response->head.response_start = base::TimeTicks::Now();
  response->head.encoded_data_length = request->GetTotalReceivedBytes();
}

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const ResourceRequestBody::Element& element)
      : net::UploadBytesElementReader(element.bytes(), element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_BYTES, element.type());
  }

  ~BytesElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const ResourceRequestBody::Element& element)
      : net::UploadFileElementReader(task_runner,
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_FILE, element.type());
  }

  ~FileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

// A subclass of net::UploadElementReader to read data pipes.
class DataPipeElementReader : public net::UploadElementReader {
 public:
  DataPipeElementReader(mojo::ScopedDataPipeConsumerHandle data_pipe,
                        storage::mojom::SizeGetterPtr size_getter)
      : data_pipe_(std::move(data_pipe)),
        handle_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        size_getter_(std::move(size_getter)),
        weak_factory_(this) {
    size_getter_->GetSize(base::Bind(&DataPipeElementReader::GetSizeCallback,
                                     weak_factory_.GetWeakPtr()));
    handle_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                          base::Bind(&DataPipeElementReader::OnHandleReadable,
                                     base::Unretained(this)));
  }

  ~DataPipeElementReader() override {}

 private:
  void GetSizeCallback(uint64_t size) {
    calculated_size_ = true;
    size_ = size;
    if (!init_callback_.is_null())
      std::move(init_callback_).Run(net::OK);
  }

  void OnHandleReadable(MojoResult result) {
    if (result == MOJO_RESULT_OK) {
      int read = Read(buf_.get(), buf_length_, net::CompletionCallback());
      DCHECK_GT(read, 0);
      std::move(read_callback_).Run(read);
    } else {
      std::move(read_callback_).Run(net::ERR_FAILED);
    }
    buf_ = nullptr;
    buf_length_ = 0;
  }

  // net::UploadElementReader implementation:
  int Init(const net::CompletionCallback& callback) override {
    if (calculated_size_)
      return net::OK;

    init_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }

  uint64_t GetContentLength() const override { return size_; }

  uint64_t BytesRemaining() const override { return size_ - bytes_read_; }

  int Read(net::IOBuffer* buf,
           int buf_length,
           const net::CompletionCallback& callback) override {
    if (!BytesRemaining())
      return net::OK;

    uint32_t num_bytes = buf_length;
    MojoResult rv =
        data_pipe_->ReadData(buf->data(), &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    if (rv == MOJO_RESULT_OK) {
      bytes_read_ += num_bytes;
      return num_bytes;
    }

    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      buf_ = buf;
      buf_length_ = buf_length;
      handle_watcher_.ArmOrNotify();
      read_callback_ = std::move(callback);
      return net::ERR_IO_PENDING;
    }

    return net::ERR_FAILED;
  }

  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher handle_watcher_;
  scoped_refptr<net::IOBuffer> buf_;
  int buf_length_ = 0;
  storage::mojom::SizeGetterPtr size_getter_;
  bool calculated_size_ = false;
  uint64_t size_ = 0;
  uint64_t bytes_read_ = 0;
  net::CompletionCallback init_callback_;
  net::CompletionCallback read_callback_;

  base::WeakPtrFactory<DataPipeElementReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataPipeElementReader);
};

// TODO: copied from content/browser/loader/upload_data_stream_builder.cc.
std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    base::SequencedTaskRunner* file_task_runner) {
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        element_readers.push_back(
            base::MakeUnique<BytesElementReader>(body, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        element_readers.push_back(base::MakeUnique<FileElementReader>(
            body, file_task_runner, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        NOTIMPLEMENTED();
        break;
      case ResourceRequestBody::Element::TYPE_BLOB: {
        NOTREACHED();
        break;
      }
      case ResourceRequestBody::Element::TYPE_DATA_PIPE: {
        storage::mojom::SizeGetterPtr size_getter;
        mojo::ScopedDataPipeConsumerHandle data_pipe =
            const_cast<storage::DataElement*>(&element)->ReleaseDataPipe(
                &size_getter);
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            std::move(data_pipe), std::move(size_getter)));
        break;
      }
      case ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY:
      case ResourceRequestBody::Element::TYPE_BYTES_DESCRIPTION:
      case ResourceRequestBody::Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  return base::MakeUnique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

}  // namespace

URLLoaderImpl::URLLoaderImpl(
    NetworkContext* context,
    mojom::URLLoaderRequest url_loader_request,
    int32_t options,
    const ResourceRequest& request,
    bool report_raw_headers,
    mojom::URLLoaderClientPtr url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : context_(context),
      options_(options),
      connected_(true),
      binding_(this, std::move(url_loader_request)),
      url_loader_client_(std::move(url_loader_client)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      peer_closed_handle_watcher_(FROM_HERE,
                                  mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      report_raw_headers_(report_raw_headers),
      weak_ptr_factory_(this) {
  context_->RegisterURLLoader(this);
  binding_.set_connection_error_handler(base::BindOnce(
      &URLLoaderImpl::OnConnectionError, base::Unretained(this)));

  url_request_ = context_->url_request_context()->CreateRequest(
      GURL(request.url), net::DEFAULT_PRIORITY, this, traffic_annotation);
  url_request_->set_method(request.method);

  url_request_->set_site_for_cookies(request.site_for_cookies);

  const Referrer referrer(request.referrer, request.referrer_policy);
  Referrer::SetReferrerForRequest(url_request_.get(), referrer);

  url_request_->SetExtraRequestHeaders(request.headers);

  // Resolve elements from request_body and prepare upload data.
  if (request.request_body.get()) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    url_request_->set_upload(
        CreateUploadDataStream(request.request_body.get(), task_runner.get()));
  }

  int load_flags = BuildLoadFlagsForRequest(request, false);
  url_request_->SetLoadFlags(load_flags);
  if (report_raw_headers_) {
    url_request_->SetRequestHeadersCallback(
        base::Bind(&net::HttpRawRequestHeaders::Assign,
                   base::Unretained(&raw_request_headers_)));
    url_request_->SetResponseHeadersCallback(base::Bind(
        &URLLoaderImpl::SetRawResponseHeaders, base::Unretained(this)));
  }
  url_request_->Start();
}

URLLoaderImpl::~URLLoaderImpl() {
  context_->DeregisterURLLoader(this);

  if (update_body_read_before_paused_)
    UpdateBodyReadBeforePaused();
  if (body_read_before_paused_ != -1) {
    if (!body_may_be_from_cache_) {
      UMA_HISTOGRAM_COUNTS_1M("Network.URLLoader.BodyReadFromNetBeforePaused",
                              body_read_before_paused_);
    } else {
      DVLOG(1) << "The request has been paused, but "
               << "Network.URLLoader.BodyReadFromNetBeforePaused is not "
               << "reported because the response body may be from cache. "
               << "body_read_before_paused_: " << body_read_before_paused_;
    }
  }
}

void URLLoaderImpl::Cleanup() {
  // The associated network context is going away and we have to destroy
  // net::URLRequest held by this loader.
  delete this;
}

void URLLoaderImpl::FollowRedirect() {
  if (!url_request_) {
    NotifyCompleted(net::ERR_UNEXPECTED);
    // |this| may have been deleted.
    return;
  }

  url_request_->FollowDeferredRedirect();
}

void URLLoaderImpl::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void URLLoaderImpl::PauseReadingBodyFromNet() {
  DVLOG(1) << "URLLoaderImpl pauses fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");
  // Please note that we pause reading body in all cases. Even if the URL
  // request indicates that the response was cached, there could still be
  // network activity involved. For example, the response was only partially
  // cached.
  //
  // On the other hand, we only report BodyReadFromNetBeforePaused histogram
  // when we are sure that the response body hasn't been read from cache. This
  // avoids polluting the histogram data with data points from cached responses.
  should_pause_reading_body_ = true;

  if (url_request_ && url_request_->status().is_io_pending()) {
    update_body_read_before_paused_ = true;
  } else {
    UpdateBodyReadBeforePaused();
  }
}

void URLLoaderImpl::ResumeReadingBodyFromNet() {
  DVLOG(1) << "URLLoaderImpl resumes fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");
  should_pause_reading_body_ = false;

  if (paused_reading_body_) {
    paused_reading_body_ = false;
    ReadMore();
  }
}

void URLLoaderImpl::OnReceivedRedirect(net::URLRequest* url_request,
                                       const net::RedirectInfo& redirect_info,
                                       bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());
  DCHECK(url_request->status().is_success());

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  scoped_refptr<ResourceResponse> response = new ResourceResponse();
  PopulateResourceResponse(url_request_.get(), response.get());
  if (report_raw_headers_) {
    response->head.devtools_info = BuildDevToolsInfo(
        *url_request_, raw_request_headers_, raw_response_headers_.get());
    raw_request_headers_ = net::HttpRawRequestHeaders();
    raw_response_headers_ = nullptr;
  }
  url_loader_client_->OnReceiveRedirect(redirect_info, response->head);
}

void URLLoaderImpl::OnResponseStarted(net::URLRequest* url_request,
                                      int net_error) {
  DCHECK(url_request == url_request_.get());

  if (net_error != net::OK) {
    NotifyCompleted(net_error);
    // |this| may have been deleted.
    return;
  }

  response_ = new ResourceResponse();
  PopulateResourceResponse(url_request_.get(), response_.get());
  if (report_raw_headers_) {
    response_->head.devtools_info = BuildDevToolsInfo(
        *url_request_, raw_request_headers_, raw_response_headers_.get());
    raw_request_headers_ = net::HttpRawRequestHeaders();
    raw_response_headers_ = nullptr;
  }

  body_may_be_from_cache_ = url_request_->was_cached();

  mojo::DataPipe data_pipe(kDefaultAllocationSize);
  response_body_stream_ = std::move(data_pipe.producer_handle);
  consumer_handle_ = std::move(data_pipe.consumer_handle);
  peer_closed_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::Bind(&URLLoaderImpl::OnResponseBodyStreamConsumerClosed,
                 base::Unretained(this)));
  peer_closed_handle_watcher_.ArmOrNotify();

  writable_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::Bind(&URLLoaderImpl::OnResponseBodyStreamReady,
                 base::Unretained(this)));

  if (!(options_ & mojom::kURLLoadOptionSniffMimeType) ||
      !ShouldSniffContent(url_request_.get(), response_.get()))
    SendResponseToClient();

  // Start reading...
  ReadMore();
}

void URLLoaderImpl::ReadMore() {
  // Once the MIME type is sniffed, all data is sent as soon as it is read from
  // the network.
  DCHECK(consumer_handle_.is_valid() || !pending_write_);

  if (should_pause_reading_body_) {
    paused_reading_body_ = true;
    return;
  }

  if (!pending_write_.get()) {
    // TODO: we should use the abstractions in MojoAsyncResourceHandler.
    DCHECK_EQ(0u, pending_write_buffer_offset_);
    MojoResult result = network::NetToMojoPendingBuffer::BeginWrite(
        &response_body_stream_, &pending_write_, &pending_write_buffer_size_);
    if (result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT) {
      // The response body stream is in a bad state. Bail.
      // TODO: How should this be communicated to our client?
      CloseResponseBodyStreamProducer();
      return;
    }

    DCHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()),
              pending_write_buffer_size_);
    if (consumer_handle_.is_valid()) {
      DCHECK_GE(pending_write_buffer_size_,
                static_cast<uint32_t>(net::kMaxBytesToSniff));
    }
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // The pipe is full. We need to wait for it to have more space.
      writable_handle_watcher_.ArmOrNotify();
      return;
    }
  }

  auto buf = base::MakeRefCounted<network::NetToMojoIOBuffer>(
      pending_write_.get(), pending_write_buffer_offset_);
  int bytes_read;
  url_request_->Read(buf.get(),
                     static_cast<int>(pending_write_buffer_size_ -
                                      pending_write_buffer_offset_),
                     &bytes_read);
  if (url_request_->status().is_io_pending()) {
    // Wait for OnReadCompleted.
  } else {
    DidRead(bytes_read, true);
    // |this| may have been deleted.
  }
}

void URLLoaderImpl::DidRead(int num_bytes, bool completed_synchronously) {
  if (num_bytes > 0)
    pending_write_buffer_offset_ += num_bytes;
  if (update_body_read_before_paused_) {
    update_body_read_before_paused_ = false;
    UpdateBodyReadBeforePaused();
  }

  bool complete_read = true;
  if (consumer_handle_.is_valid()) {
    const std::string& type_hint = response_->head.mime_type;
    std::string new_type;
    bool made_final_decision = net::SniffMimeType(
        pending_write_->buffer(), pending_write_buffer_offset_,
        url_request_->url(), type_hint, &new_type);
    // SniffMimeType() returns false if there is not enough data to determine
    // the mime type. However, even if it returns false, it returns a new type
    // that is probably better than the current one.
    response_->head.mime_type.assign(new_type);

    if (made_final_decision) {
      SendResponseToClient();
    } else {
      complete_read = false;
    }
  }

  if (!url_request_->status().is_success() || num_bytes == 0) {
    CompletePendingWrite();
    NotifyCompleted(url_request_->status().ToNetError());

    CloseResponseBodyStreamProducer();
    // |this| may have been deleted.
    return;
  }

  if (complete_read) {
    CompletePendingWrite();
  }
  if (completed_synchronously) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoaderImpl::ReadMore,
                                  weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void URLLoaderImpl::OnReadCompleted(net::URLRequest* url_request,
                                    int bytes_read) {
  DCHECK(url_request == url_request_.get());

  DidRead(bytes_read, false);
  // |this| may have been deleted.
}

base::WeakPtr<URLLoaderImpl> URLLoaderImpl::GetWeakPtrForTests() {
  return weak_ptr_factory_.GetWeakPtr();
}

void URLLoaderImpl::NotifyCompleted(int error_code) {
  if (consumer_handle_.is_valid())
    SendResponseToClient();

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.exists_in_cache =
      url_request_->response_info().was_cached;
  request_complete_data.completion_time = base::TimeTicks::Now();
  request_complete_data.encoded_data_length =
      url_request_->GetTotalReceivedBytes();
  request_complete_data.encoded_body_length = url_request_->GetRawBodyBytes();
  request_complete_data.decoded_body_length = total_written_bytes_;

  url_loader_client_->OnComplete(request_complete_data);
  DeleteIfNeeded();
}

void URLLoaderImpl::OnConnectionError() {
  connected_ = false;
  DeleteIfNeeded();
}

void URLLoaderImpl::OnResponseBodyStreamConsumerClosed(MojoResult result) {
  CloseResponseBodyStreamProducer();
}

void URLLoaderImpl::OnResponseBodyStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    CloseResponseBodyStreamProducer();
    return;
  }

  ReadMore();
}

void URLLoaderImpl::CloseResponseBodyStreamProducer() {
  url_request_.reset();
  peer_closed_handle_watcher_.Cancel();
  writable_handle_watcher_.Cancel();
  response_body_stream_.reset();

  // |pending_write_buffer_offset_| is intentionally not reset, so that
  // |total_written_bytes_ + pending_write_buffer_offset_| always reflects the
  // total bytes read from |url_request_|.
  pending_write_ = nullptr;

  // Make sure if a ResumeReadingBodyFromNet() call is received later, we don't
  // try to do ReadMore().
  paused_reading_body_ = false;

  DeleteIfNeeded();
}

void URLLoaderImpl::DeleteIfNeeded() {
  bool has_data_pipe = pending_write_.get() || response_body_stream_.is_valid();
  if (!connected_ && !has_data_pipe)
    delete this;
}

void URLLoaderImpl::SendResponseToClient() {
  base::Optional<net::SSLInfo> ssl_info;
  if (options_ & mojom::kURLLoadOptionSendSSLInfo)
    ssl_info = url_request_->ssl_info();
  mojom::DownloadedTempFilePtr downloaded_file_ptr;
  url_loader_client_->OnReceiveResponse(response_->head, ssl_info,
                                        std::move(downloaded_file_ptr));

  net::IOBufferWithSize* metadata =
      url_request_->response_info().metadata.get();
  if (metadata) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(metadata->data());

    url_loader_client_->OnReceiveCachedMetadata(
        std::vector<uint8_t>(data, data + metadata->size()));
  }

  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle_));
  response_ = nullptr;
}

void URLLoaderImpl::CompletePendingWrite() {
  response_body_stream_ =
      pending_write_->Complete(pending_write_buffer_offset_);
  total_written_bytes_ += pending_write_buffer_offset_;
  pending_write_ = nullptr;
  pending_write_buffer_offset_ = 0;
}

void URLLoaderImpl::SetRawResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  raw_response_headers_ = headers;
}

void URLLoaderImpl::UpdateBodyReadBeforePaused() {
  DCHECK_GE(pending_write_buffer_offset_ + total_written_bytes_,
            body_read_before_paused_);
  body_read_before_paused_ =
      pending_write_buffer_offset_ + total_written_bytes_;
}

}  // namespace content
