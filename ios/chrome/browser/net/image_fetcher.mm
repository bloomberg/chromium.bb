// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/image_fetcher.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/mac/scoped_nsobject.h"
#include "base/task_runner.h"
#include "ios/chrome/browser/webp_transcode/webp_decoder.h"
#include "ios/web/public/web_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class WebpDecoderDelegate : public webp_transcode::WebpDecoder::Delegate {
 public:
  NSData* data() const { return decoded_image_; }

  // WebpDecoder::Delegate methods
  void OnFinishedDecoding(bool success) override {
    if (!success)
      decoded_image_.reset();
  }
  void SetImageFeatures(
      size_t total_size,
      webp_transcode::WebpDecoder::DecodedImageFormat format) override {
    decoded_image_.reset([[NSMutableData alloc] initWithCapacity:total_size]);
  }
  void OnDataDecoded(NSData* data) override {
    DCHECK(decoded_image_);
    [decoded_image_ appendData:data];
  }
 private:
  ~WebpDecoderDelegate() override {}
  base::scoped_nsobject<NSMutableData> decoded_image_;
};

// Content-type header for WebP images.
static const char kWEBPMimeType[] = "image/webp";

// Returns a NSData object containing the decoded image.
// Returns nil in case of failure.
base::scoped_nsobject<NSData> DecodeWebpImage(
    const base::scoped_nsobject<NSData>& webp_image) {
  scoped_refptr<WebpDecoderDelegate> delegate(new WebpDecoderDelegate);
  scoped_refptr<webp_transcode::WebpDecoder> decoder(
      new webp_transcode::WebpDecoder(delegate.get()));
  decoder->OnDataReceived(webp_image);
  DLOG_IF(ERROR, !delegate->data()) << "WebP image decoding failed.";
  return base::scoped_nsobject<NSData>(delegate->data());
}

}  // namespace

ImageFetcher::ImageFetcher(const scoped_refptr<base::TaskRunner>& task_runner)
    : request_context_getter_(nullptr),
      task_runner_(task_runner),
      weak_factory_(this) {
  DCHECK(task_runner_.get());
}

ImageFetcher::~ImageFetcher() {
  // Delete all the entries in the |downloads_in_progress_| map.  This will in
  // turn cancel all of the requests.
  for (const auto& pair : downloads_in_progress_) {
    delete pair.first;
  }
}

void ImageFetcher::StartDownload(
    const GURL& url,
    ImageFetchedCallback callback,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy) {
  DCHECK(request_context_getter_.get());
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, this).release();
  downloads_in_progress_[fetcher] = [callback copy];
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetReferrer(referrer);
  fetcher->SetReferrerPolicy(referrer_policy);
  fetcher->Start();
}

void ImageFetcher::StartDownload(
    const GURL& url, ImageFetchedCallback callback) {
  ImageFetcher::StartDownload(
      url, callback, std::string(), net::URLRequest::NEVER_CLEAR_REFERRER);
}

// Delegate callback that is called when URLFetcher completes.  If the image
// was fetched successfully, creates a new NSData and returns it to the
// callback, otherwise returns nil to the callback.
void ImageFetcher::OnURLFetchComplete(const net::URLFetcher* fetcher) {
  if (downloads_in_progress_.find(fetcher) == downloads_in_progress_.end()) {
    LOG(ERROR) << "Received callback for unknown URLFetcher " << fetcher;
    return;
  }

  // Ensures that |fetcher| will be deleted in the event of early return.
  std::unique_ptr<const net::URLFetcher> fetcher_deleter(fetcher);

  // Retrieves the callback and ensures that it will be deleted in the event
  // of early return.
  base::mac::ScopedBlock<ImageFetchedCallback> callback(
      downloads_in_progress_[fetcher]);

  // Remove |fetcher| from the map.
  downloads_in_progress_.erase(fetcher);

  // Make sure the request was successful. For "data" requests, the response
  // code has no meaning, because there is no actual server (data is encoded
  // directly in the URL). In that case, set the response code to 200 (OK).
  const GURL& original_url = fetcher->GetOriginalURL();
  const int http_response_code = original_url.SchemeIs("data") ?
      200 : fetcher->GetResponseCode();
  if (http_response_code != 200) {
    (callback.get())(original_url, http_response_code, nil);
    return;
  }

  std::string response;
  if (!fetcher->GetResponseAsString(&response)) {
    (callback.get())(original_url, http_response_code, nil);
    return;
  }

  // Create a NSData from the returned data and notify the callback.
  base::scoped_nsobject<NSData> data([[NSData alloc]
      initWithBytes:reinterpret_cast<const unsigned char*>(response.data())
             length:response.size()]);

  if (fetcher->GetResponseHeaders()) {
    std::string mime_type;
    fetcher->GetResponseHeaders()->GetMimeType(&mime_type);
    if (mime_type == kWEBPMimeType) {
      base::PostTaskAndReplyWithResult(task_runner_.get(),
                                       FROM_HERE,
                                       base::Bind(&DecodeWebpImage, data),
                                       base::Bind(&ImageFetcher::RunCallback,
                                                  weak_factory_.GetWeakPtr(),
                                                  callback,
                                                  original_url,
                                                  http_response_code));
      return;
    }
  }
  (callback.get())(original_url, http_response_code, data);
}

void ImageFetcher::RunCallback(
    const base::mac::ScopedBlock<ImageFetchedCallback>& callback,
    const GURL& url,
    int http_response_code,
    NSData* data) {
  (callback.get())(url, http_response_code, data);
}

void ImageFetcher::SetRequestContextGetter(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter) {
  request_context_getter_ = request_context_getter;
}
