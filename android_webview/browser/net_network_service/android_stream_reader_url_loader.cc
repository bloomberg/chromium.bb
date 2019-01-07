// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net_network_service/android_stream_reader_url_loader.h"

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/net/input_stream_reader.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace android_webview {

namespace {

const char kResponseHeaderViaShouldInterceptRequest[] =
    "Client-Via: shouldInterceptRequest";
const char kHTTPOkText[] = "OK";
const char kHTTPNotFoundText[] = "Not Found";

}  // namespace

// In the case when stream reader related tasks are posted on a dedicated
// thread they can outlive the loader. This is a wrapper is for holding both
// InputStream and InputStreamReader to ensure they are still there when the
// task is run.
class InputStreamReaderWrapper
    : public base::RefCountedThreadSafe<InputStreamReaderWrapper> {
 public:
  InputStreamReaderWrapper(
      std::unique_ptr<InputStream> input_stream,
      std::unique_ptr<InputStreamReader> input_stream_reader)
      : input_stream_(std::move(input_stream)),
        input_stream_reader_(std::move(input_stream_reader)) {
    DCHECK(input_stream_);
    DCHECK(input_stream_reader_);
  }

  InputStream* input_stream() { return input_stream_.get(); }

  int Seek(const net::HttpByteRange& byte_range) {
    return input_stream_reader_->Seek(byte_range);
  }

  int ReadRawData(net::IOBuffer* buffer, int buffer_size) {
    return input_stream_reader_->ReadRawData(buffer, buffer_size);
  }

 private:
  friend class base::RefCountedThreadSafe<InputStreamReaderWrapper>;
  ~InputStreamReaderWrapper() {}

  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<InputStreamReader> input_stream_reader_;

  DISALLOW_COPY_AND_ASSIGN(InputStreamReaderWrapper);
};

class AndroidResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  AndroidResponseDelegate(std::unique_ptr<AwWebResourceResponse> response)
      : response_(std::move(response)) {}
  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return response_->GetInputStream(env);
  }

 private:
  std::unique_ptr<AwWebResourceResponse> response_;
};

AndroidStreamReaderURLLoader::AndroidStreamReaderURLLoader(
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    std::unique_ptr<ResponseDelegate> response_delegate)
    : resource_request_(resource_request),
      client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      response_delegate_(std::move(response_delegate)),
      weak_factory_(this) {
  // If there is a client error, clean up the request.
  client_.set_connection_error_handler(base::BindOnce(
      &AndroidStreamReaderURLLoader::OnRequestError, weak_factory_.GetWeakPtr(),
      network::URLLoaderCompletionStatus(net::ERR_ABORTED)));
}

AndroidStreamReaderURLLoader::~AndroidStreamReaderURLLoader() {}

void AndroidStreamReaderURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {}
void AndroidStreamReaderURLLoader::ProceedWithResponse() {}
void AndroidStreamReaderURLLoader::SetPriority(net::RequestPriority priority,
                                               int intra_priority_value) {}
void AndroidStreamReaderURLLoader::PauseReadingBodyFromNet() {}
void AndroidStreamReaderURLLoader::ResumeReadingBodyFromNet() {}

void AndroidStreamReaderURLLoader::Start() {
  if (!ParseRange(resource_request_.headers)) {
    OnRequestError(network::URLLoaderCompletionStatus(
        net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  // TODO(timvolodine): keep the original threading behavior (as in
  // AndroidStreamReaderURLRequestJob) and open the stream on a dedicated
  // thread. (crbug.com/913524).
  std::unique_ptr<InputStream> input_stream =
      response_delegate_->OpenInputStream(env);
  OnInputStreamOpened(std::move(input_stream));
}

void AndroidStreamReaderURLLoader::OnInputStreamOpened(
    std::unique_ptr<InputStream> input_stream) {
  if (!input_stream) {
    // restart not required
    HeadersComplete(net::HTTP_NOT_FOUND, kHTTPNotFoundText);
    return;
  }

  auto input_stream_reader =
      std::make_unique<InputStreamReader>(input_stream.get());
  DCHECK(input_stream);
  DCHECK(!input_stream_reader_wrapper_);

  input_stream_reader_wrapper_ = base::MakeRefCounted<InputStreamReaderWrapper>(
      std::move(input_stream), std::move(input_stream_reader));

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&InputStreamReaderWrapper::Seek,
                     input_stream_reader_wrapper_, byte_range_),
      base::BindOnce(&AndroidStreamReaderURLLoader::OnReaderSeekCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLLoader::OnReaderSeekCompleted(int result) {
  if (result >= 0) {
    // we've got the expected content size here
    HeadersComplete(net::HTTP_OK, kHTTPOkText);
  } else {
    OnRequestError(network::URLLoaderCompletionStatus(net::ERR_FAILED));
  }
}

// TODO(timvolodine): move this to delegate to make this more generic,
// to also support streams other than for shouldInterceptRequest.
void AndroidStreamReaderURLLoader::HeadersComplete(
    int status_code,
    const std::string& status_text) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(status_text);
  // HttpResponseHeaders expects its input string to be terminated by two NULs.
  status.append("\0\0", 2);

  network::ResourceResponseHead head;
  head.request_start = base::TimeTicks::Now();
  head.response_start = base::TimeTicks::Now();
  head.headers = new net::HttpResponseHeaders(status);

  // TODO(timvolodine): add content length header
  // TODO(timvolodine): add proper mime information

  // Indicate that the response had been obtained via shouldInterceptRequest.
  head.headers->AddHeader(kResponseHeaderViaShouldInterceptRequest);

  DCHECK(client_.is_bound());
  client_->OnReceiveResponse(head);

  if (status_code != net::HTTP_OK) {
    OnRequestError(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  SendBody();
}

void AndroidStreamReaderURLLoader::SendBody() {
  // TODO(timvolodine): implement IOBuffer stream -> mojo pipes here
  mojo::ScopedDataPipeConsumerHandle body_to_send;
  client_->OnStartLoadingResponseBody(std::move(body_to_send));
  RequestComplete(network::URLLoaderCompletionStatus(net::OK));
}

void AndroidStreamReaderURLLoader::RequestComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    OnRequestError(status);
    return;
  }
  client_->OnComplete(status);

  // Manages its own lifetime
  delete this;
}

void AndroidStreamReaderURLLoader::OnRequestError(
    const network::URLLoaderCompletionStatus& status) {
  client_->OnComplete(status);

  // Manages it's own lifetime
  delete this;
}

// TODO(timvolodine): consider moving this to the net_helpers.cc
bool AndroidStreamReaderURLLoader::ParseRange(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // This loader only cares about the Range header so that we know how many
    // bytes in the stream to skip and how many to read after that.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      // In case of multi-range request only use the first range.
      // We don't support multirange requests.
      if (ranges.size() == 1)
        byte_range_ = ranges[0];
    } else {
      // This happens if the range header could not be parsed or is invalid.
      return false;
    }
  }
  return true;
}

}  // namespace android_webview
