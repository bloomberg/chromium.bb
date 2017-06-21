// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_info_loader.h"

#include <utility>

#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoader.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebAssociatedURLLoader;
using blink::WebAssociatedURLLoaderOptions;
using blink::WebFrame;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

static const int kHttpOK = 200;
static const int kHttpPartialContentOK = 206;

MediaInfoLoader::MediaInfoLoader(
    const GURL& url,
    blink::WebMediaPlayer::CORSMode cors_mode,
    const ReadyCB& ready_cb)
    : loader_failed_(false),
      url_(url),
      allow_stored_credentials_(false),
      cors_mode_(cors_mode),
      single_origin_(true),
      ready_cb_(ready_cb) {}

MediaInfoLoader::~MediaInfoLoader() {}

void MediaInfoLoader::Start(blink::WebFrame* frame) {
  // Make sure we have not started.
  DCHECK(!ready_cb_.is_null());
  CHECK(frame);

  start_time_ = base::TimeTicks::Now();
  first_party_url_ = frame->GetDocument().FirstPartyForCookies();

  // Prepare the request.
  WebURLRequest request(url_);
  // TODO(mkwst): Split this into video/audio.
  request.SetRequestContext(WebURLRequest::kRequestContextVideo);
  frame->SetReferrerForRequest(request, blink::WebURL());

  // Since we don't actually care about the media data at this time, use a two
  // byte range request to avoid unnecessarily downloading resources.  Not all
  // servers support HEAD unfortunately, so use a range request; which is no
  // worse than the previous request+cancel code.  See http://crbug.com/400788
  request.AddHTTPHeaderField("Range", "bytes=0-1");

  std::unique_ptr<WebAssociatedURLLoader> loader;
  if (test_loader_) {
    loader = std::move(test_loader_);
  } else {
    WebAssociatedURLLoaderOptions options;
    if (cors_mode_ == blink::WebMediaPlayer::kCORSModeUnspecified) {
      request.SetFetchCredentialsMode(
          WebURLRequest::kFetchCredentialsModeInclude);
      options.fetch_request_mode = WebURLRequest::kFetchRequestModeNoCORS;
      allow_stored_credentials_ = true;
    } else {
      options.expose_all_response_headers = true;
      // The author header set is empty, no preflight should go ahead.
      options.preflight_policy =
          WebAssociatedURLLoaderOptions::kPreventPreflight;
      options.fetch_request_mode = WebURLRequest::kFetchRequestModeCORS;
      if (cors_mode_ == blink::WebMediaPlayer::kCORSModeUseCredentials) {
        request.SetFetchCredentialsMode(
            WebURLRequest::kFetchCredentialsModeInclude);
        allow_stored_credentials_ = true;
      } else {
        request.SetFetchCredentialsMode(
            WebURLRequest::kFetchCredentialsModeSameOrigin);
      }
    }
    loader.reset(frame->CreateAssociatedURLLoader(options));
  }

  // Start the resource loading.
  loader->LoadAsynchronously(request, this);
  active_loader_.reset(new media::ActiveLoader(std::move(loader)));
}

/////////////////////////////////////////////////////////////////////////////
// blink::WebAssociatedURLLoaderClient implementation.
bool MediaInfoLoader::WillFollowRedirect(
    const WebURLRequest& newRequest,
    const WebURLResponse& redirectResponse) {
  // The load may have been stopped and |ready_cb| is destroyed.
  // In this case we shouldn't do anything.
  if (ready_cb_.is_null())
    return false;

  // Only allow |single_origin_| if we haven't seen a different origin yet.
  if (single_origin_)
    single_origin_ = url_.GetOrigin() == GURL(newRequest.Url()).GetOrigin();

  url_ = newRequest.Url();
  first_party_url_ = newRequest.FirstPartyForCookies();
  allow_stored_credentials_ = newRequest.AllowStoredCredentials();

  return true;
}

void MediaInfoLoader::DidSendData(unsigned long long bytes_sent,
                                  unsigned long long total_bytes_to_be_sent) {
  NOTIMPLEMENTED();
}

void MediaInfoLoader::DidReceiveResponse(const WebURLResponse& response) {
  DVLOG(1) << "didReceiveResponse: HTTP/"
           << (response.HttpVersion() == WebURLResponse::kHTTPVersion_0_9
                   ? "0.9"
                   : response.HttpVersion() == WebURLResponse::kHTTPVersion_1_0
                         ? "1.0"
                         : response.HttpVersion() ==
                                   WebURLResponse::kHTTPVersion_1_1
                               ? "1.1"
                               : "Unknown")
           << " " << response.HttpStatusCode();
  DCHECK(active_loader_.get());
  if (!url_.SchemeIs(url::kHttpScheme) && !url_.SchemeIs(url::kHttpsScheme)) {
      DidBecomeReady(kOk);
      return;
  }
  if (response.HttpStatusCode() == kHttpOK ||
      response.HttpStatusCode() == kHttpPartialContentOK) {
    DidBecomeReady(kOk);
    return;
  }
  loader_failed_ = true;
  DidBecomeReady(kFailed);
}

void MediaInfoLoader::DidReceiveData(const char* data, int data_length) {
  // Ignored.
}

void MediaInfoLoader::DidDownloadData(int dataLength) {
  NOTIMPLEMENTED();
}

void MediaInfoLoader::DidReceiveCachedMetadata(const char* data,
                                               int data_length) {
  NOTIMPLEMENTED();
}

void MediaInfoLoader::DidFinishLoading(double finishTime) {
  DCHECK(active_loader_.get());
  DidBecomeReady(kOk);
}

void MediaInfoLoader::DidFail(const WebURLError& error) {
  DVLOG(1) << "didFail: reason=" << error.reason
           << ", isCancellation=" << error.is_cancellation
           << ", domain=" << error.domain.Utf8().data()
           << ", localizedDescription="
           << error.localized_description.Utf8().data();
  DCHECK(active_loader_.get());
  loader_failed_ = true;
  DidBecomeReady(kFailed);
}

bool MediaInfoLoader::HasSingleOrigin() const {
  DCHECK(ready_cb_.is_null())
      << "Must become ready before calling HasSingleOrigin()";
  return single_origin_;
}

bool MediaInfoLoader::DidPassCORSAccessCheck() const {
  DCHECK(ready_cb_.is_null())
      << "Must become ready before calling DidPassCORSAccessCheck()";
  return !loader_failed_ &&
         cors_mode_ != blink::WebMediaPlayer::kCORSModeUnspecified;
}

/////////////////////////////////////////////////////////////////////////////
// Helper methods.

void MediaInfoLoader::DidBecomeReady(Status status) {
  UMA_HISTOGRAM_TIMES("Media.InfoLoadDelay",
                      base::TimeTicks::Now() - start_time_);
  active_loader_.reset();
  if (!ready_cb_.is_null())
    base::ResetAndReturn(&ready_cb_).Run(status, url_, first_party_url_,
                                         allow_stored_credentials_);
}

}  // namespace content
