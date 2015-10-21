// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_url_loader_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoadTiming.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

using blink::WebString;
using mojo::URLResponsePtr;

namespace html_viewer {
namespace {

blink::WebURLResponse::HTTPVersion StatusLineToHTTPVersion(
    const mojo::String& status_line) {
  if (status_line.is_null())
    return blink::WebURLResponse::HTTPVersion_0_9;

  if (base::StartsWith(status_line.get(), "HTTP/1.0",
                       base::CompareCase::SENSITIVE))
    return blink::WebURLResponse::HTTPVersion_1_0;

  if (base::StartsWith(status_line.get(), "HTTP/1.1",
                       base::CompareCase::SENSITIVE))
    return blink::WebURLResponse::HTTPVersion_1_1;

  return blink::WebURLResponse::HTTPVersionUnknown;
}

blink::WebURLResponse ToWebURLResponse(const URLResponsePtr& url_response) {
  blink::WebURLResponse result;
  result.initialize();
  result.setURL(GURL(url_response->url));
  result.setMIMEType(blink::WebString::fromUTF8(url_response->mime_type));
  result.setTextEncodingName(blink::WebString::fromUTF8(url_response->charset));
  result.setHTTPVersion(StatusLineToHTTPVersion(url_response->status_line));
  result.setHTTPStatusCode(url_response->status_code);
  result.setExpectedContentLength(-1);  // Not available.

  // TODO(darin): Initialize timing properly.
  blink::WebURLLoadTiming timing;
  timing.initialize();
  result.setLoadTiming(timing);

  for (size_t i = 0; i < url_response->headers.size(); ++i) {
    result.setHTTPHeaderField(
        blink::WebString::fromUTF8(url_response->headers[i]->name),
        blink::WebString::fromUTF8(url_response->headers[i]->value));
  }

  return result;
}

}  // namespace

WebURLRequestExtraData::WebURLRequestExtraData() {
}

WebURLRequestExtraData::~WebURLRequestExtraData() {
}

WebURLLoaderImpl::WebURLLoaderImpl(mojo::URLLoaderFactory* url_loader_factory,
                                   MockWebBlobRegistryImpl* web_blob_registry)
    : client_(NULL),
      web_blob_registry_(web_blob_registry),
      referrer_policy_(blink::WebReferrerPolicyDefault),
      weak_factory_(this) {
  url_loader_factory->CreateURLLoader(GetProxy(&url_loader_));
}

WebURLLoaderImpl::~WebURLLoaderImpl() {
}

void WebURLLoaderImpl::loadSynchronously(
    const blink::WebURLRequest& request,
    blink::WebURLResponse& response,
    blink::WebURLError& error,
    blink::WebData& data) {
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  url_request->auto_follow_redirects = true;
  URLResponsePtr url_response;
  url_loader_->Start(url_request.Pass(),
                     [&url_response](URLResponsePtr url_response_result) {
                        url_response = url_response_result.Pass();
                     });
  url_loader_.WaitForIncomingResponse();
  if (url_response->error) {
    error.domain = WebString::fromUTF8(net::kErrorDomain);
    error.reason = url_response->error->code;
    error.unreachableURL = GURL(url_response->url);
    return;
  }

  response = ToWebURLResponse(url_response);
  std::string body;
  mojo::common::BlockingCopyToString(url_response->body.Pass(), &body);
  data.assign(body.data(), body.length());
}

void WebURLLoaderImpl::loadAsynchronously(const blink::WebURLRequest& request,
                                          blink::WebURLLoaderClient* client) {
  client_ = client;
  url_ = request.url();

  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  url_request->auto_follow_redirects = false;
  referrer_policy_ = request.referrerPolicy();

  if (request.extraData()) {
    WebURLRequestExtraData* extra_data =
        static_cast<WebURLRequestExtraData*>(request.extraData());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&WebURLLoaderImpl::OnReceivedResponse,
                   weak_factory_.GetWeakPtr(),
                   request,
                   base::Passed(&extra_data->synthetic_response)));
    return;
  }

  blink::WebString uuid;
  if (web_blob_registry_->GetUUIDForURL(url_, &uuid)) {
    blink::WebVector<blink::WebBlobData::Item*> items;
    if (web_blob_registry_->GetBlobItems(uuid, &items)) {
      // The blob data exists in our service, and we don't want to create a
      // data pipe just to do a funny dance where at the end, we stuff data
      // from memory into data pipes so we can read back the data.
      OnReceiveWebBlobData(request, items);
      return;
    }
  }

  url_loader_->Start(url_request.Pass(),
                     base::Bind(&WebURLLoaderImpl::OnReceivedResponse,
                                weak_factory_.GetWeakPtr(), request));
}

void WebURLLoaderImpl::cancel() {
  url_loader_.reset();
  response_body_stream_.reset();

  URLResponsePtr failed_response(mojo::URLResponse::New());
  failed_response->url = mojo::String::From(url_);
  failed_response->error = mojo::NetworkError::New();
  failed_response->error->code = net::ERR_ABORTED;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WebURLLoaderImpl::OnReceivedResponse,
                 weak_factory_.GetWeakPtr(),
                 blink::WebURLRequest(),
                 base::Passed(&failed_response)));
}

void WebURLLoaderImpl::setDefersLoading(bool defers_loading) {
  NOTIMPLEMENTED();
}

void WebURLLoaderImpl::OnReceivedResponse(const blink::WebURLRequest& request,
                                          URLResponsePtr url_response) {
  url_ = GURL(url_response->url);

  if (url_response->error) {
    OnReceivedError(url_response.Pass());
  } else if (url_response->redirect_url) {
    OnReceivedRedirect(request, url_response.Pass());
  } else {
    base::WeakPtr<WebURLLoaderImpl> self(weak_factory_.GetWeakPtr());
    client_->didReceiveResponse(this, ToWebURLResponse(url_response));

    // We may have been deleted during didReceiveResponse.
    if (!self)
      return;

    // Start streaming data
    response_body_stream_ = url_response->body.Pass();
    ReadMore();
  }
}

void WebURLLoaderImpl::OnReceivedError(URLResponsePtr url_response) {
  blink::WebURLError web_error;
  web_error.domain = blink::WebString::fromUTF8(net::kErrorDomain);
  web_error.reason = url_response->error->code;
  web_error.unreachableURL = GURL(url_response->url);
  web_error.staleCopyInCache = false;
  web_error.isCancellation =
      url_response->error->code == net::ERR_ABORTED ? true : false;

  client_->didFail(this, web_error);
}

void WebURLLoaderImpl::OnReceivedRedirect(const blink::WebURLRequest& request,
                                          URLResponsePtr url_response) {
  // TODO(erg): setFirstPartyForCookies() and setHTTPReferrer() are unset here.
  blink::WebURLRequest new_request;
  new_request.initialize();
  new_request.setURL(GURL(url_response->redirect_url));
  new_request.setDownloadToFile(request.downloadToFile());
  new_request.setRequestContext(request.requestContext());
  new_request.setFrameType(request.frameType());
  new_request.setSkipServiceWorker(request.skipServiceWorker());
  new_request.setFetchRequestMode(request.fetchRequestMode());
  new_request.setFetchCredentialsMode(request.fetchCredentialsMode());
  new_request.setHTTPReferrer(
      WebString::fromUTF8(url_response->redirect_referrer),
      referrer_policy_);

  std::string old_method = request.httpMethod().utf8();
  new_request.setHTTPMethod(
      blink::WebString::fromUTF8(url_response->redirect_method));
  if (url_response->redirect_method == old_method)
    new_request.setHTTPBody(request.httpBody());

  base::WeakPtr<WebURLLoaderImpl> self(weak_factory_.GetWeakPtr());
  client_->willFollowRedirect(
      this, new_request, ToWebURLResponse(url_response));
  // TODO(darin): Check if new_request was rejected.

  // We may have been deleted during willFollowRedirect.
  if (!self)
    return;

  url_loader_->FollowRedirect(
      base::Bind(&WebURLLoaderImpl::OnReceivedResponse,
                 weak_factory_.GetWeakPtr(),
                 request));
}

void WebURLLoaderImpl::OnReceiveWebBlobData(
    const blink::WebURLRequest& request,
    const blink::WebVector<blink::WebBlobData::Item*>& items) {
  blink::WebURLResponse result;
  result.initialize();
  result.setURL(url_);
  result.setHTTPStatusCode(200);
  result.setExpectedContentLength(-1);  // Not available.

  base::WeakPtr<WebURLLoaderImpl> self(weak_factory_.GetWeakPtr());
  client_->didReceiveResponse(this, result);

  // We may have been deleted during didReceiveResponse.
  if (!self)
    return;

  // Send a receive data for each blob item.
  for (size_t i = 0; i < items.size(); ++i) {
    const int data_size = base::checked_cast<int>(items[i]->data.size());
    client_->didReceiveData(this, items[i]->data.data(), data_size, -1);
  }

  // Send a closing finish.
  double finish_time = base::Time::Now().ToDoubleT();
  client_->didFinishLoading(
      this, finish_time, blink::WebURLLoaderClient::kUnknownEncodedDataLength);
}

void WebURLLoaderImpl::ReadMore() {
  const void* buf;
  uint32_t buf_size;
  MojoResult rv = BeginReadDataRaw(response_body_stream_.get(),
                                   &buf,
                                   &buf_size,
                                   MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    base::WeakPtr<WebURLLoaderImpl> self(weak_factory_.GetWeakPtr());
    client_->didReceiveData(this, static_cast<const char*>(buf), buf_size, -1);
    // We may have been deleted during didReceiveData.
    if (!self)
      return;
    EndReadDataRaw(response_body_stream_.get(), buf_size);
    WaitToReadMore();
  } else if (rv == MOJO_RESULT_SHOULD_WAIT) {
    WaitToReadMore();
  } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    // We reached end-of-file.
    double finish_time = base::Time::Now().ToDoubleT();
    client_->didFinishLoading(
        this,
        finish_time,
        blink::WebURLLoaderClient::kUnknownEncodedDataLength);
  } else {
    // TODO(darin): Oops!
  }
}

void WebURLLoaderImpl::WaitToReadMore() {
  handle_watcher_.Start(
      response_body_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      base::Bind(&WebURLLoaderImpl::OnResponseBodyStreamReady,
                 weak_factory_.GetWeakPtr()));
}

void WebURLLoaderImpl::OnResponseBodyStreamReady(MojoResult result) {
  ReadMore();
}

void WebURLLoaderImpl::setLoadingTaskRunner(
    blink::WebTaskRunner* web_task_runner) {
  // TODO(alexclarke): Consider hooking this up.
}

}  // namespace html_viewer
