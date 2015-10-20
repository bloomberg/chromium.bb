// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_static_file_web_view.h"

#include "base/logging.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/ui_web_view_util.h"
#include "net/http/http_response_headers.h"

namespace {
NSString* const kStaticFileUserAgent = @"UIWebViewForStaticFileContent";
}

@interface CRWStaticFileWebView () {
  // Saves the user agent in order to set it on future requests.
  base::scoped_nsobject<NSString> userAgent_;
  // Saves the identification assigned to this web view for request
  // tracking. This identifiation is required for terminating request
  // tracking.
  scoped_refptr<web::RequestTrackerImpl> requestTracker_;
}

// Whether the User-Agent is the allowed user agent.
+ (BOOL)isStaticFileUserAgent:(NSString*)userAgent;
@end

@implementation CRWStaticFileWebView

- (instancetype)initWithFrame:(CGRect)frame
                 browserState:(web::BrowserState*)browserState {
  // Register the user agent before instantiating the UIWebView.
  NSString* requestGroupID = nil;
  NSString* userAgent = kStaticFileUserAgent;
  if (browserState) {
    requestGroupID = web::GenerateNewRequestGroupID();
    userAgent = web::AddRequestGroupIDToUserAgent(userAgent, requestGroupID);
  }
  web::RegisterUserAgentForUIWebView(userAgent);
  self = [super initWithFrame:frame];
  if (self) {
    DCHECK(!browserState || [requestGroupID length]);
    if (browserState) {
      userAgent_.reset([userAgent copy]);
      // If |browserState| is not nullptr, associate this UIWebView with the
      // given |requestGroupID| which cannot be nil or zero-length.
      // RequestTracker keeps track of requests to this requestGroupID until
      // this UIWebView is deallocated. UIWebViews not associated with a
      // requestGroupID will issue requests in the global request context.
      base::scoped_nsobject<NSString> requestGroupIDCopy([requestGroupID copy]);
      requestTracker_ = web::RequestTrackerImpl::CreateTrackerForRequestGroupID(
          requestGroupIDCopy,
          browserState,
          browserState->GetRequestContext(),
          self);
    }
  }
  return self;
}

- (void)dealloc {
  if (requestTracker_.get())
    requestTracker_->Close();
  [super dealloc];
}

+ (BOOL)isStaticFileRequest:(NSURLRequest*)request {
  NSString* userAgent = [request allHTTPHeaderFields][@"User-Agent"];
  if (userAgent) {
    return [CRWStaticFileWebView isStaticFileUserAgent:userAgent];
  }

  // If a request originated from another file:/// page, the User-Agent
  // is not available. In this case, check that the request is for image
  // resources only.
  NSString* suffix = [[request URL] pathExtension];
  return [@[ @"png", @"jpg", @"jpeg" ] containsObject:[suffix lowercaseString]];
}

+ (BOOL)isStaticFileUserAgent:(NSString*)userAgent {
  return [userAgent hasPrefix:kStaticFileUserAgent];
}

#pragma mark -
#pragma mark UIWebView

// On iOS 6.0, UIWebView does not set the user agent for static file requests.
// The solution consists of overriding |loadRequest:| and setting the user agent
// before loading the request.
- (void)loadRequest:(NSURLRequest*)request {
  base::scoped_nsobject<NSMutableURLRequest> mutableRequest(
      (NSMutableURLRequest*)[request mutableCopy]);
  [mutableRequest setValue:userAgent_ forHTTPHeaderField:@"User-Agent"];
  [super loadRequest:mutableRequest];
}

#pragma mark -
#pragma mark CRWRequestTrackerDelegate

- (BOOL)isForStaticFileRequests {
  return YES;
}

- (void)handleResponseHeaders:(net::HttpResponseHeaders*)headers
                   requestUrl:(const GURL&)requestUrl {
  std::string mimeType;
  DCHECK(headers->GetMimeType(&mimeType) && mimeType == "text/html");
  DCHECK(!headers->HasHeader("X-Auto-Login"));
  DCHECK(!headers->HasHeader("content-disposition"));
}

// The following delegate functions are not expected to be called since
// the page is intended to be a static HTML page.

- (void)updatedSSLStatus:(const web::SSLStatus&)sslStatus
              forPageUrl:(const GURL&)url
                userInfo:(id)userInfo {
  NOTREACHED();
}

- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
                  onUrl:(const GURL&)url
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue {
  NOTREACHED();
}

- (void)updatedProgress:(float)progress {
  NOTREACHED();
}

- (void)certificateUsed:(net::X509Certificate*)certificate
                forHost:(const std::string&)host
                 status:(net::CertStatus)status {
  NOTREACHED();
}

- (void)clearCertificates {
  NOTREACHED();
}

@end
