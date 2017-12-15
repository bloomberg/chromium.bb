// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/url_loader.h"

#include <string>

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/common/loader_util.h"
#include "content/network/data_pipe_element_reader.h"
#include "content/network/network_context.h"
#include "content/network/network_service_impl.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/mime_sniffer.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cert/symantec_certs.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content {

namespace {
constexpr size_t kDefaultAllocationSize = 512 * 1024;

// TODO: this duplicates some of PopulateResourceResponse in
// content/browser/loader/resource_loader.cc
void PopulateResourceResponse(net::URLRequest* request,
                              bool is_load_timing_enabled,
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

  if (is_load_timing_enabled)
    request->GetLoadTimingInfo(&response->head.load_timing);

  if (request->ssl_info().cert.get()) {
    response->head.is_legacy_symantec_cert =
        (!net::IsCertStatusError(response->head.cert_status) ||
         net::IsCertStatusMinorError(response->head.cert_status)) &&
        net::IsLegacySymantecCert(request->ssl_info().public_key_hashes);
    response->head.cert_validity_start =
        request->ssl_info().cert->valid_start();
    response->head.cert_status = request->ssl_info().cert_status;
  }

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

class RawFileElementReader : public net::UploadFileElementReader {
 public:
  RawFileElementReader(ResourceRequestBody* resource_request_body,
                       base::TaskRunner* task_runner,
                       const ResourceRequestBody::Element& element)
      : net::UploadFileElementReader(
            task_runner,
            // TODO(mmenke): Is duplicating this necessary?
            element.file().Duplicate(),
            element.path(),
            element.offset(),
            element.length(),
            element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_RAW_FILE, element.type());
  }

  ~RawFileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(RawFileElementReader);
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
            std::make_unique<BytesElementReader>(body, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element));
        break;
      case ResourceRequestBody::Element::TYPE_RAW_FILE:
        element_readers.push_back(std::make_unique<RawFileElementReader>(
            body, file_task_runner, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        CHECK(false) << "Should never be reached";
        break;
      case ResourceRequestBody::Element::TYPE_BLOB: {
        CHECK(false) << "Network service always uses DATA_PIPE for blobs.";
        break;
      }
      case ResourceRequestBody::Element::TYPE_DATA_PIPE: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body, const_cast<storage::DataElement*>(&element)
                      ->ReleaseDataPipeGetter()));
        break;
      }
      case ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY:
      case ResourceRequestBody::Element::TYPE_BYTES_DESCRIPTION:
      case ResourceRequestBody::Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

}  // namespace

URLLoader::URLLoader(NetworkContext* context,
                     mojom::URLLoaderRequest url_loader_request,
                     int32_t options,
                     const ResourceRequest& request,
                     bool report_raw_headers,
                     mojom::URLLoaderClientPtr url_loader_client,
                     const net::NetworkTrafficAnnotationTag& traffic_annotation,
                     uint32_t process_id)
    : context_(context),
      options_(options),
      resource_type_(request.resource_type),
      is_load_timing_enabled_(request.enable_load_timing),
      process_id_(process_id),
      render_frame_id_(request.render_frame_id),
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
  binding_.set_connection_error_handler(
      base::BindOnce(&URLLoader::OnConnectionError, base::Unretained(this)));

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

    if (request.enable_upload_progress) {
      upload_progress_tracker_ = std::make_unique<UploadProgressTracker>(
          FROM_HERE,
          base::BindRepeating(&URLLoader::SendUploadProgress,
                              base::Unretained(this)),
          url_request_.get());
    }
  }

  url_request_->set_initiator(request.request_initiator);

  // TODO(qinmin): network service shouldn't know about resource type, need
  // to introduce another field to set this.
  if (request.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    url_request_->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  int load_flags = BuildLoadFlagsForRequest(request, false);
  url_request_->SetLoadFlags(load_flags);
  if (report_raw_headers_) {
    url_request_->SetRequestHeadersCallback(
        base::Bind(&net::HttpRawRequestHeaders::Assign,
                   base::Unretained(&raw_request_headers_)));
    url_request_->SetResponseHeadersCallback(
        base::Bind(&URLLoader::SetRawResponseHeaders, base::Unretained(this)));
  }

  AttachAcceptHeader(request.resource_type, url_request_.get());

  url_request_->Start();
}

URLLoader::~URLLoader() {
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

void URLLoader::Cleanup() {
  // The associated network context is going away and we have to destroy
  // net::URLRequest held by this loader.
  delete this;
}

void URLLoader::FollowRedirect() {
  if (!url_request_) {
    NotifyCompleted(net::ERR_UNEXPECTED);
    // |this| may have been deleted.
    return;
  }

  url_request_->FollowDeferredRedirect();
}

void URLLoader::SetPriority(net::RequestPriority priority,
                            int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void URLLoader::PauseReadingBodyFromNet() {
  DVLOG(1) << "URLLoader pauses fetching response body for "
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

void URLLoader::ResumeReadingBodyFromNet() {
  DVLOG(1) << "URLLoader resumes fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");
  should_pause_reading_body_ = false;

  if (paused_reading_body_) {
    paused_reading_body_ = false;
    ReadMore();
  }
}

void URLLoader::OnReceivedRedirect(net::URLRequest* url_request,
                                   const net::RedirectInfo& redirect_info,
                                   bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());
  DCHECK(url_request->status().is_success());

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  scoped_refptr<ResourceResponse> response = new ResourceResponse();
  PopulateResourceResponse(url_request_.get(), is_load_timing_enabled_,
                           response.get());
  if (report_raw_headers_) {
    response->head.devtools_info = BuildDevToolsInfo(
        *url_request_, raw_request_headers_, raw_response_headers_.get());
    raw_request_headers_ = net::HttpRawRequestHeaders();
    raw_response_headers_ = nullptr;
  }
  url_loader_client_->OnReceiveRedirect(redirect_info, response->head);
}

void URLLoader::OnAuthRequired(net::URLRequest* unused,
                               net::AuthChallengeInfo* auth_info) {
  NOTIMPLEMENTED() << "http://crbug.com/756654";
  net::URLRequest::Delegate::OnAuthRequired(unused, auth_info);
}

void URLLoader::OnCertificateRequested(net::URLRequest* unused,
                                       net::SSLCertRequestInfo* cert_info) {
  NOTIMPLEMENTED() << "http://crbug.com/756654";
  net::URLRequest::Delegate::OnCertificateRequested(unused, cert_info);
}

void URLLoader::OnSSLCertificateError(net::URLRequest* request,
                                      const net::SSLInfo& ssl_info,
                                      bool fatal) {
  // The network service can be null in tests.
  if (!context_->network_service()) {
    OnSSLCertificateErrorResponse(ssl_info, net::ERR_INSECURE_RESPONSE);
    return;
  }
  context_->network_service()->client()->OnSSLCertificateError(
      resource_type_, url_request_->url(), process_id_, render_frame_id_,
      ssl_info, fatal,
      base::Bind(&URLLoader::OnSSLCertificateErrorResponse,
                 weak_ptr_factory_.GetWeakPtr(), ssl_info));
}

void URLLoader::OnResponseStarted(net::URLRequest* url_request, int net_error) {
  DCHECK(url_request == url_request_.get());

  if (net_error != net::OK) {
    NotifyCompleted(net_error);
    // |this| may have been deleted.
    return;
  }

  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  response_ = new ResourceResponse();
  PopulateResourceResponse(url_request_.get(), is_load_timing_enabled_,
                           response_.get());
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
      base::Bind(&URLLoader::OnResponseBodyStreamConsumerClosed,
                 base::Unretained(this)));
  peer_closed_handle_watcher_.ArmOrNotify();

  writable_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::Bind(&URLLoader::OnResponseBodyStreamReady,
                 base::Unretained(this)));

  if (!(options_ & mojom::kURLLoadOptionSniffMimeType) ||
      !ShouldSniffContent(url_request_.get(), response_.get()))
    SendResponseToClient();

  // Start reading...
  ReadMore();
}

void URLLoader::ReadMore() {
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

void URLLoader::DidRead(int num_bytes, bool completed_synchronously) {
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
        FROM_HERE,
        base::BindOnce(&URLLoader::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void URLLoader::OnReadCompleted(net::URLRequest* url_request, int bytes_read) {
  DCHECK(url_request == url_request_.get());

  DidRead(bytes_read, false);
  // |this| may have been deleted.
}

base::WeakPtr<URLLoader> URLLoader::GetWeakPtrForTests() {
  return weak_ptr_factory_.GetWeakPtr();
}

void URLLoader::NotifyCompleted(int error_code) {
  // Ensure sending the final upload progress message here, since
  // OnResponseCompleted can be called without OnResponseStarted on cancellation
  // or error cases.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  if (consumer_handle_.is_valid())
    SendResponseToClient();

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.exists_in_cache = url_request_->response_info().was_cached;
  status.completion_time = base::TimeTicks::Now();
  status.encoded_data_length = url_request_->GetTotalReceivedBytes();
  status.encoded_body_length = url_request_->GetRawBodyBytes();
  status.decoded_body_length = total_written_bytes_;

  if ((options_ & mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
      net::IsCertStatusError(url_request_->ssl_info().cert_status) &&
      !net::IsCertStatusMinorError(url_request_->ssl_info().cert_status)) {
    status.ssl_info = url_request_->ssl_info();
  }

  url_loader_client_->OnComplete(status);
  DeleteIfNeeded();
}

void URLLoader::OnConnectionError() {
  connected_ = false;
  DeleteIfNeeded();
}

void URLLoader::OnResponseBodyStreamConsumerClosed(MojoResult result) {
  CloseResponseBodyStreamProducer();
}

void URLLoader::OnResponseBodyStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    CloseResponseBodyStreamProducer();
    return;
  }

  ReadMore();
}

void URLLoader::CloseResponseBodyStreamProducer() {
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

void URLLoader::DeleteIfNeeded() {
  bool has_data_pipe = pending_write_.get() || response_body_stream_.is_valid();
  if (!connected_ && !has_data_pipe)
    delete this;
}

void URLLoader::SendResponseToClient() {
  base::Optional<net::SSLInfo> ssl_info;
  if (options_ & mojom::kURLLoadOptionSendSSLInfoWithResponse)
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

void URLLoader::CompletePendingWrite() {
  response_body_stream_ =
      pending_write_->Complete(pending_write_buffer_offset_);
  total_written_bytes_ += pending_write_buffer_offset_;
  pending_write_ = nullptr;
  pending_write_buffer_offset_ = 0;
}

void URLLoader::SetRawResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  raw_response_headers_ = headers;
}

void URLLoader::UpdateBodyReadBeforePaused() {
  DCHECK_GE(pending_write_buffer_offset_ + total_written_bytes_,
            body_read_before_paused_);
  body_read_before_paused_ =
      pending_write_buffer_offset_ + total_written_bytes_;
}

void URLLoader::SendUploadProgress(const net::UploadProgress& progress) {
  url_loader_client_->OnUploadProgress(
      progress.position(), progress.size(),
      base::BindOnce(&URLLoader::OnUploadProgressACK,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnUploadProgressACK() {
  if (upload_progress_tracker_)
    upload_progress_tracker_->OnAckReceived();
}

void URLLoader::OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                              int net_error) {
  // The request can be NULL if it was cancelled by the client.
  if (!url_request_ || !url_request_->is_pending())
    return;

  if (net_error == net::OK) {
    url_request_->ContinueDespiteLastError();
    return;
  }

  url_request_->CancelWithSSLError(net_error, ssl_info);
}

}  // namespace content
