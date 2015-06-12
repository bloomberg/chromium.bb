// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_H_
#define IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_H_

#import <Foundation/Foundation.h>

namespace net {
class URLRequestContextGetter;

class HTTPProtocolHandlerDelegate {
 public:
  // Sets the global instance of the HTTPProtocolHandlerDelegate.
  static void SetInstance(HTTPProtocolHandlerDelegate* delegate);

  virtual ~HTTPProtocolHandlerDelegate() {}

  // Returns true if CRNHTTPProtocolHandler should handle the request.
  // Returns false if the request should be passed down the NSURLProtocol chain.
  virtual bool CanHandleRequest(NSURLRequest* request) = 0;

  // If IsRequestSupported returns true, |request| will be processed, otherwise
  // a NSURLErrorUnsupportedURL error is generated.
  virtual bool IsRequestSupported(NSURLRequest* request) = 0;

  // Returns the request context used for requests that are not associated with
  // a RequestTracker. This includes in particular the requests that are not
  // aware of the network stack. Must not return null.
  virtual URLRequestContextGetter* GetDefaultURLRequestContext() = 0;
};

}  // namespace net

// Custom NSURLProtocol handling HTTP and HTTPS requests.
// The HttpProtocolHandler is registered as a NSURLProtocol in the iOS system.
// This protocol is called for each NSURLRequest. This allows handling the
// requests issued by UIWebView using our own network stack.
@interface CRNHTTPProtocolHandler : NSURLProtocol
@end

// Custom NSURLProtocol handling HTTP and HTTPS requests.
// The HttpProtocolHandler is registered as a NSURLProtocol in the iOS system.
// This protocol is called for each NSURLRequest. This allows handling the
// requests issued by UIWebView using our own network stack. This protocol
// handler implements iOS 8 NSURLProtocol pause/resume semantics, in which
// |startLoading| means "start or resume" and |stopLoading| means "pause".
@interface CRNPauseableHTTPProtocolHandler : CRNHTTPProtocolHandler
@end

#endif  // IOS_NET_CRN_HTTP_PROTOCOL_HANDLER_H_
