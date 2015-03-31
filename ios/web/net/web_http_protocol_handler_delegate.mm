// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/web_http_protocol_handler_delegate.h"

#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_static_file_web_view.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace {

bool IsAppSpecificScheme(NSURL* url) {
  NSString* scheme = [url scheme];
  if (![scheme length])
    return false;
  // Use the GURL implementation, but with a scheme-only URL to avoid
  // unnecessary parsing in GURL construction.
  GURL gurl([[scheme stringByAppendingString:@":"] UTF8String]);
  return web::GetWebClient()->IsAppSpecificURL(gurl);
}

}  // namespace

namespace web {

WebHTTPProtocolHandlerDelegate::WebHTTPProtocolHandlerDelegate(
    net::URLRequestContextGetter* default_getter)
    : default_getter_(default_getter) {
  DCHECK(default_getter_);
}

WebHTTPProtocolHandlerDelegate::~WebHTTPProtocolHandlerDelegate() {
}

bool WebHTTPProtocolHandlerDelegate::CanHandleRequest(NSURLRequest* request) {
  // Accept all the requests. If we declined a request, it would then be passed
  // to the default iOS network stack, which would possibly load it.
  // As we want to control what is loaded, we have to prevent the default stack
  // from loading anything.
  return true;
}

bool WebHTTPProtocolHandlerDelegate::IsRequestSupported(NSURLRequest* request) {
  return web::UrlHasWebScheme([request URL]) ||
         [CRWStaticFileWebView isStaticFileRequest:request] ||
         (IsAppSpecificScheme([request URL]) &&
          IsAppSpecificScheme([request mainDocumentURL]));
}

net::URLRequestContextGetter*
WebHTTPProtocolHandlerDelegate::GetDefaultURLRequestContext() {
  return default_getter_.get();
}

}  // namespace web
