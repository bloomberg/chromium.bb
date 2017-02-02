// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"

#import "base/mac/bind_objc_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#import "ios/web/public/image_fetcher/webp_decoder.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - WebpDecoderDelegate

namespace {

// TODO(crbug.com/687921): Refactor this.
class WebpDecoderDelegate : public webp_transcode::WebpDecoder::Delegate {
 public:
  WebpDecoderDelegate() = default;

  NSData* data() const { return decoded_image_; }

  // WebpDecoder::Delegate methods
  void OnFinishedDecoding(bool success) override {
    if (!success)
      decoded_image_ = nil;
  }
  void SetImageFeatures(
      size_t total_size,
      webp_transcode::WebpDecoder::DecodedImageFormat format) override {
    decoded_image_ = [[NSMutableData alloc] initWithCapacity:total_size];
  }
  void OnDataDecoded(NSData* data) override {
    DCHECK(decoded_image_);
    [decoded_image_ appendData:data];
  }

 private:
  ~WebpDecoderDelegate() override = default;
  NSMutableData* decoded_image_;

  DISALLOW_COPY_AND_ASSIGN(WebpDecoderDelegate);
};

// Content-type header for WebP images.
const char kWEBPFirstMagicPattern[] = "RIFF";
const char kWEBPSecondMagicPattern[] = "WEBP";

// Returns a NSData object containing the decoded image.
// Returns nil in case of failure.
NSData* DecodeWebpImage(NSData* webp_image) {
  scoped_refptr<WebpDecoderDelegate> delegate(new WebpDecoderDelegate);
  scoped_refptr<webp_transcode::WebpDecoder> decoder(
      new webp_transcode::WebpDecoder(delegate.get()));
  decoder->OnDataReceived(webp_image);
  DLOG_IF(ERROR, !delegate->data()) << "WebP image decoding failed.";
  return delegate->data();
}

}  // namespace

#pragma mark - IOSImageDataFetcherWrapper

namespace image_fetcher {

IOSImageDataFetcherWrapper::IOSImageDataFetcherWrapper(
    net::URLRequestContextGetter* url_request_context_getter,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : task_runner_(task_runner),
      image_data_fetcher_(url_request_context_getter) {
  DCHECK(task_runner_.get());
}

IOSImageDataFetcherWrapper::~IOSImageDataFetcherWrapper() {}

void IOSImageDataFetcherWrapper::FetchImageDataWebpDecoded(
    const GURL& image_url,
    IOSImageDataFetcherCallback callback) {
  DCHECK(callback);

  scoped_refptr<base::TaskRunner> task_runner = task_runner_;
  ImageDataFetcher::ImageDataFetcherCallback local_callback =
      base::BindBlockArc(^(const std::string& image_data) {
        // Create a NSData from the returned data and notify the callback.
        NSData* data =
            [NSData dataWithBytes:image_data.data() length:image_data.size()];

        if (data.length < 12 ||
            image_data.compare(0, 4, kWEBPFirstMagicPattern) != 0 ||
            image_data.compare(8, 4, kWEBPSecondMagicPattern) != 0) {
          callback(data);
          return;
        }

        // The image is a webp image.
        base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE,
                                         base::Bind(&DecodeWebpImage, data),
                                         base::BindBlockArc(callback));
      });
  image_data_fetcher_.FetchImageData(image_url, local_callback);
}

void IOSImageDataFetcherWrapper::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  image_data_fetcher_.SetDataUseServiceName(data_use_service_name);
}

}  // namespace image_fetcher
