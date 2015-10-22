// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webp_transcode/webp_network_client.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/webp_transcode/webp_decoder.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace net {
class URLRequest;
}

using namespace webp_transcode;

namespace {

// MIME type for WebP images.
const char kWebPMimeType[] = "image/webp";
NSString* const kNSWebPMimeType = @"image/webp";

NSURLResponse* NewImageResponse(NSURLResponse* webp_response,
                                size_t content_length,
                                WebpDecoder::DecodedImageFormat format) {
  DCHECK(webp_response);

  NSString* mime_type = nil;
  switch (format) {
    case WebpDecoder::JPEG:
      mime_type = @"image/jpeg";
      break;
    case WebpDecoder::PNG:
      mime_type = @"image/png";
      break;
    case WebpDecoder::TIFF:
      mime_type = @"image/tiff";
      break;
    case WebpDecoder::DECODED_FORMAT_COUNT:
      NOTREACHED();
      break;
  }
  DCHECK(mime_type);

  if ([webp_response isKindOfClass:[NSHTTPURLResponse class]]) {
    NSHTTPURLResponse* http_response =
        static_cast<NSHTTPURLResponse*>(webp_response);
    NSMutableDictionary* header_fields = [NSMutableDictionary
        dictionaryWithDictionary:[http_response allHeaderFields]];
    [header_fields setObject:[NSString stringWithFormat:@"%zu", content_length]
                      forKey:@"Content-Length"];
    [header_fields setObject:mime_type forKey:@"Content-Type"];
    return [[NSHTTPURLResponse alloc] initWithURL:[http_response URL]
                                       statusCode:[http_response statusCode]
                                      HTTPVersion:@"HTTP/1.1"
                                     headerFields:header_fields];
  } else {
    return [[NSURLResponse alloc] initWithURL:[webp_response URL]
                                     MIMEType:mime_type
                        expectedContentLength:content_length
                             textEncodingName:[webp_response textEncodingName]];
  }
}

class WebpDecoderDelegate : public WebpDecoder::Delegate {
 public:
  WebpDecoderDelegate(id<CRNNetworkClientProtocol> client,
                      const base::Time& request_creation_time,
                      const scoped_refptr<base::TaskRunner>& callback_runner)
      : underlying_client_([client retain]),
        callback_task_runner_(callback_runner),
        request_creation_time_(request_creation_time) {
    DCHECK(underlying_client_.get());
  }

  void SetOriginalResponse(
      const base::scoped_nsobject<NSURLResponse>& response) {
    original_response_.reset([response retain]);
  }

  // WebpDecoder::Delegate methods.
  void OnFinishedDecoding(bool success) override {
    base::scoped_nsprotocol<id<CRNNetworkClientProtocol>> block_client(
        [underlying_client_ retain]);
    if (success) {
      callback_task_runner_->PostTask(FROM_HERE, base::BindBlock(^{
        [block_client didFinishLoading];
      }));
    } else {
      DLOG(WARNING) << "WebP decoding failed "
                    << base::SysNSStringToUTF8(
                           [[original_response_ URL] absoluteString]);
      void (^errorBlock)(void) = ^{
        [block_client didFailWithNSErrorCode:NSURLErrorCannotDecodeContentData
                                netErrorCode:net::ERR_CONTENT_DECODING_FAILED];
      };
      callback_task_runner_->PostTask(FROM_HERE, base::BindBlock(errorBlock));
    }
  }

  void SetImageFeatures(size_t total_size,
                        WebpDecoder::DecodedImageFormat format) override {
    base::scoped_nsobject<NSURLResponse> imageResponse(
        NewImageResponse(original_response_, total_size, format));
    DCHECK(imageResponse);
    base::scoped_nsprotocol<id<CRNNetworkClientProtocol>> block_client(
        [underlying_client_ retain]);
    callback_task_runner_->PostTask(FROM_HERE, base::BindBlock(^{
      [block_client didReceiveResponse:imageResponse];
    }));
  }

  void OnDataDecoded(NSData* data) override {
    base::scoped_nsprotocol<id<CRNNetworkClientProtocol>> block_client(
        [underlying_client_ retain]);
    callback_task_runner_->PostTask(FROM_HERE, base::BindBlock(^{
      [block_client didLoadData:data];
    }));
  }

 private:
  ~WebpDecoderDelegate() override {}

  base::scoped_nsprotocol<id<CRNNetworkClientProtocol>> underlying_client_;
  base::scoped_nsobject<NSURLResponse> original_response_;
  scoped_refptr<base::TaskRunner> callback_task_runner_;
  base::Time request_creation_time_;
};

}  // namespace

@interface WebPNetworkClient () {
  scoped_refptr<webp_transcode::WebpDecoder> _webpDecoder;
  scoped_refptr<WebpDecoderDelegate> _webpDecoderDelegate;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  base::Time _requestCreationTime;
}
@end

@implementation WebPNetworkClient

- (instancetype)init {
  NOTREACHED() << "Use |-initWithTaskRunner:| instead";
  return nil;
}

- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)runner {
  if (self = [super init]) {
    DCHECK(runner);
    _taskRunner = runner;
  }
  return self;
}

- (void)didCreateNativeRequest:(net::URLRequest*)nativeRequest {
  // Append 'image/webp' to the outgoing 'Accept' header.
  const net::HttpRequestHeaders& headers =
      nativeRequest->extra_request_headers();
  std::string acceptHeader;
  if (headers.GetHeader("Accept", &acceptHeader)) {
    // Add 'image/webp' if it isn't in the Accept header yet.
    if (acceptHeader.find(kWebPMimeType) == std::string::npos) {
      acceptHeader += std::string(",") + kWebPMimeType;
      nativeRequest->SetExtraRequestHeaderByName("Accept", acceptHeader, true);
    }
  } else {
    // All requests should already have an Accept: header, so this case
    // should never happen outside of unit tests.
    nativeRequest->SetExtraRequestHeaderByName("Accept", kWebPMimeType, false);
  }
  [super didCreateNativeRequest:nativeRequest];
}

- (void)didLoadData:(NSData*)data {
  if (_webpDecoder.get()) {
    // |data| is assumed to be immutable.
    base::scoped_nsobject<NSData> scopedData([data retain]);
    _taskRunner->PostTask(FROM_HERE, base::Bind(&WebpDecoder::OnDataReceived,
                                                _webpDecoder, scopedData));
  } else {
    [super didLoadData:data];
  }
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  DCHECK(self.underlyingClient);
  NSString* responseMimeType = [response MIMEType];
  if (responseMimeType &&
      [responseMimeType caseInsensitiveCompare:kNSWebPMimeType] ==
          NSOrderedSame) {
    _webpDecoderDelegate =
        new WebpDecoderDelegate(self.underlyingClient, _requestCreationTime,
                                base::ThreadTaskRunnerHandle::Get());
    _webpDecoder = new webp_transcode::WebpDecoder(_webpDecoderDelegate.get());
    base::scoped_nsobject<NSURLResponse> scoped_response([response copy]);
    _taskRunner->PostTask(FROM_HERE,
                          base::Bind(&WebpDecoderDelegate::SetOriginalResponse,
                                     _webpDecoderDelegate, scoped_response));
    // Do not call super here, the WebpDecoderDelegate will update the mime type
    // and call |-didReceiveResponse:|.
  } else {
    // If this isn't a WebP, pass the call up the chain.
    [super didReceiveResponse:response];
  }
}

- (void)didFinishLoading {
  if (_webpDecoder.get()) {
    _taskRunner->PostTask(FROM_HERE,
                          base::Bind(&WebpDecoder::Stop, _webpDecoder));
  } else {
    [super didFinishLoading];
  }
}

@end
