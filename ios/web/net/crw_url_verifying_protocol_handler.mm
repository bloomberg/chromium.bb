// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/crw_url_verifying_protocol_handler.h"

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

// A private singleton object to hold all shared flags/data used by
// CRWURLVerifyingProtocolHandler.
@interface CRWURLVerifyingProtocolHandlerData : NSObject {
 @private
  // Flag to remember that class has been pre-initialized.
  BOOL _preInitialized;
  // Contains the last seen URL by the constructor of the ProtocolHandler.
  // This must only be accessed from the main thread.
  GURL _lastSeenURL;
  // On iOS8, |+canInitWithRequest| is not called on the main thread. Thus the
  // url check is run in |-initWithRequest| instead. See crbug.com/380768.
  // TODO(droger): Run the check at the same place on all versions.
  BOOL _runInInitWithRequest;
}
@property(nonatomic, assign) BOOL preInitialized;
@property(nonatomic, readonly) BOOL runInInitWithRequest;
// Returns the global CRWURLVerifyingProtocolHandlerData instance.
+ (CRWURLVerifyingProtocolHandlerData*)sharedInstance;
// If there is a URL saved as "last seen URL", return it as an autoreleased
// object. |newURL| is now saved as the "last seen URL".
- (GURL)swapLastSeenURL:(const GURL&)newURL;
@end

@implementation CRWURLVerifyingProtocolHandlerData
@synthesize preInitialized = _preInitialized;
@synthesize runInInitWithRequest = _runInInitWithRequest;

+ (CRWURLVerifyingProtocolHandlerData*)sharedInstance {
  static CRWURLVerifyingProtocolHandlerData* instance =
      [[CRWURLVerifyingProtocolHandlerData alloc] init];
  return instance;
}

- (GURL)swapLastSeenURL:(const GURL&)newURL {
  // Note that release() does *not* call release on oldURL.
  const GURL oldURL(_lastSeenURL);
  _lastSeenURL = newURL;
  return oldURL;
}

- (instancetype)init {
  if (self = [super init]) {
    _runInInitWithRequest = base::ios::IsRunningOnIOS8OrLater();
  }
  return self;
}

@end

namespace web {

// The special URL used to communicate between the JavaScript and this handler.
// This has to be a http request, because Ajax request can only be cross origin
// for http and https schemes. localhost:0 is used because no sane URL should
// use this. A relative URL is also used if the first request fails, because
// HTTP servers can use headers to prevent arbitrary ajax requests.
const char kURLForVerification[] = "https://localhost:0/crwebiossecurity";

}  // namespace web

namespace {

// This URL has been chosen with a specific prefix, and a random suffix to
// prevent accidental collision.
const char kCheckRelativeURL[] =
    "/crwebiossecurity/b86b97a1-2ce0-44fd-a074-e2158790c98d";

}  // namespace

@interface CRWURLVerifyingProtocolHandler () {
  // The URL of the request to handle.
  base::scoped_nsobject<NSURL> _url;
}

// Returns the JavaScript to execute to check URL.
+ (NSString*)checkURLJavaScript;
// Implements the logic for verifying the current URL for the given
// |webView|. This is the internal implementation for the public interface
// of +currentURLForWebView:trustLevel:.
+ (GURL)internalCurrentURLForWebView:(UIWebView*)webView
    trustLevel:(web::URLVerificationTrustLevel*)trustLevel;
// Updates the last seen URL to be the mainDocumentURL of |request|.
+ (void)updateLastSeenUrl:(NSURLRequest*)request;
@end

@implementation CRWURLVerifyingProtocolHandler

+ (NSString*)checkURLJavaScript {
  static base::scoped_nsobject<NSString> cachedJavaScript;
  if (!cachedJavaScript) {
    // The JavaScript to execute. It does execute an synchronous Ajax request to
    // the special URL handled by this handler and then returns the URL of the
    // UIWebView by computing window.location.href.
    // NOTE(qsr):
    // - Creating a new XMLHttpRequest can crash the application if the Web
    // Thread is iterating over active DOM objects. To prevent this from
    // happening, the same XMLHttpRequest is reused as much as possible.
    // - A XMLHttpRequest is associated to a document, and trying to reuse one
    // from another document will trigger an exception. To prevent this,
    // information about the document on which the current XMLHttpRequest has
    // been created is kept.
    cachedJavaScript.reset([[NSString
        stringWithFormat:
            @"try{"
             "window.__gCrWeb_Verifying = true;"
             "if(!window.__gCrWeb_CachedRequest||"
             "!(window.__gCrWeb_CachedRequestDocument===window.document)){"
             "window.__gCrWeb_CachedRequest = new XMLHttpRequest();"
             "window.__gCrWeb_CachedRequestDocument = window.document;"
             "}"
             "window.__gCrWeb_CachedRequest.open('POST','%s',false);"
             "window.__gCrWeb_CachedRequest.send();"
             "}catch(e){"
             "try{"
             "window.__gCrWeb_CachedRequest.open('POST','%s',false);"
             "window.__gCrWeb_CachedRequest.send();"
             "}catch(e2){}"
             "}"
             "delete window.__gCrWeb_Verifying;"
             "window.location.href",
            web::kURLForVerification, kCheckRelativeURL] retain]);
  }
  return cachedJavaScript.get();
}

// Calls +internalCurrentURLForWebView:trustLevel: to do the real work.
// Logs timing of the actual work to console for debugging.
+ (GURL)currentURLForWebView:(UIWebView*)webView
                  trustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  base::Time start = base::Time::NowFromSystemTime();
  const GURL result(
      [CRWURLVerifyingProtocolHandler internalCurrentURLForWebView:webView
                                                        trustLevel:trustLevel]);
  base::TimeDelta elapsed = base::Time::NowFromSystemTime() - start;
  UMA_HISTOGRAM_TIMES("WebController.UrlVerifyTimes", elapsed);
  // Setting pre-initialization flag to YES here disables pre-initialization
  // if pre-initialization is held back such that it is called after the
  // first real call to this function.
  [[CRWURLVerifyingProtocolHandlerData sharedInstance] setPreInitialized:YES];
  return result;
}

// The implementation of this method is doing the following
// - Set the "last seen URL" to nil.
// - Inject JavaScript in the UIWebView that will execute a synchronous ajax
//   request to |kURLForVerification|
//   - The CRWURLVerifyingProtocolHandler will then update "last seen URL" to
//     the value of the request mainDocumentURL.
// - Execute window.location.href on the UIWebView.
// - Do one of the following:
//   - If "last seen URL" is nil, return the value of window.location.href and
//     set |trustLevel| to kNone.
//   - If "last seen URL" is not nil and "last seen URL" origin is the same as
//     window.location.href origin, return window.location.href and set
//     |trustLevel| to kAbsolute.
//   - If "last seen URL" is not nil and "last seen URL" origin is *not* the
//     same as window.location.href origin, return "last seen URL" and set
//     |trustLevel| to kMixed.
// Only the origin is checked, because pushed states are not reflected in the
// mainDocumentURL of the request.
+ (GURL)internalCurrentURLForWebView:(UIWebView*)webView
    trustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  // This should only be called on the main thread. The reason is that an
  // attacker must not be able to compromise the result by generating a request
  // to |kURLForVerification| from another tab. To prevent this,
  // "last seen URL" is only updated if the handler is created on the main
  // thread, and this only happens if the JavaScript is injected from the main
  // thread too.
  DCHECK([NSThread isMainThread]);
  DCHECK(trustLevel) << "Verification of the trustLevel state is mandatory";

  // Compute the main document URL using a synchronous AJAX request.
  [[CRWURLVerifyingProtocolHandlerData sharedInstance] swapLastSeenURL:GURL()];
  // Executing the script, will set "last seen URL" as a request will be
  // executed.
  NSString* script = [CRWURLVerifyingProtocolHandler checkURLJavaScript];
  NSString* href = [webView stringByEvaluatingJavaScriptFromString:script];
  GURL nativeURL([[CRWURLVerifyingProtocolHandlerData sharedInstance]
      swapLastSeenURL:GURL()]);

  // applewebdata:// is occasionally set as the URL for a blank page during
  // transition. For instance, if <META HTTP-EQUIV="refresh" ...>' is used.
  // This results in spurious history entries if this isn't masked with the
  // default page URL of about:blank.
  if ([href hasPrefix:@"applewebdata://"])
    href = @"about:blank";
  const GURL jsURL(base::SysNSStringToUTF8(href));

  // If XHR is not working (e.g., slow PDF, XHR blocked), fall back to the
  // UIWebView request. This lags behind the other changes (it appears to update
  // at the point where the document object becomes present), so it's more
  // likely to return kMixed during transitions, but it's better than erroring
  // out when the faster XHR validation method isn't available.
  if (!nativeURL.is_valid() && webView.request) {
    nativeURL = net::GURLWithNSURL(webView.request.URL);
  }

  if (!nativeURL.is_valid()) {
    *trustLevel = web::URLVerificationTrustLevel::kNone;
    return jsURL;
  }
  if (jsURL.GetOrigin() != nativeURL.GetOrigin()) {
    DVLOG(1) << "Origin differs, trusting webkit over JavaScript ["
        << "jsURLOrigin='" << jsURL.GetOrigin() << ", "
        << "nativeURLOrigin='" << nativeURL.GetOrigin() << "']";
    *trustLevel = web::URLVerificationTrustLevel::kMixed;
    return nativeURL;
  }
  *trustLevel = web::URLVerificationTrustLevel::kAbsolute;
  return jsURL;
}

+ (void)updateLastSeenUrl:(NSURLRequest*)request {
  DCHECK([NSThread isMainThread]);
  if ([NSThread isMainThread]) {
    // See above why this should only be done if this is called on the main
    // thread.
    [[CRWURLVerifyingProtocolHandlerData sharedInstance]
        swapLastSeenURL:net::GURLWithNSURL(request.mainDocumentURL)];
  }
}

#pragma mark -
#pragma mark Class Method

// Injection of JavaScript into any UIWebView pre-initializes the entire
// system which will save run time when user types into Omnibox and triggers
// JavaScript injection again.
+ (BOOL)preInitialize {
  if ([[CRWURLVerifyingProtocolHandlerData sharedInstance] preInitialized])
    return YES;
  web::URLVerificationTrustLevel trustLevel;
  web::WebClient* web_client = web::GetWebClient();
  DCHECK(web_client);
  base::scoped_nsobject<UIWebView> dummyWebView(web::CreateStaticFileWebView());
  [CRWURLVerifyingProtocolHandler currentURLForWebView:dummyWebView
                                            trustLevel:&trustLevel];
  return [[CRWURLVerifyingProtocolHandlerData sharedInstance] preInitialized];
}

#pragma mark NSURLProtocol methods

+ (BOOL)canInitWithRequest:(NSURLRequest*)request {
  GURL requestURL = net::GURLWithNSURL(request.URL);
  if (requestURL != GURL(web::kURLForVerification) &&
      requestURL.path() != kCheckRelativeURL) {
    return NO;
  }

  if (![[CRWURLVerifyingProtocolHandlerData
              sharedInstance] runInInitWithRequest]) {
    [CRWURLVerifyingProtocolHandler updateLastSeenUrl:request];
  }

  return YES;
}

+ (NSURLRequest*)canonicalRequestForRequest:(NSURLRequest*)request {
  return request;
}

- (id)initWithRequest:(NSURLRequest*)request
       cachedResponse:(NSCachedURLResponse*)cachedResponse
               client:(id<NSURLProtocolClient>)client {
  if ((self = [super initWithRequest:request
                      cachedResponse:cachedResponse
                              client:client])) {
    if ([[CRWURLVerifyingProtocolHandlerData
                sharedInstance] runInInitWithRequest]) {
      [CRWURLVerifyingProtocolHandler updateLastSeenUrl:request];
    }

    _url.reset([request.URL retain]);
  }
  return self;
}

- (void)startLoading {
  NSMutableDictionary* headerFields = [NSMutableDictionary dictionary];
  // This request is done by an AJAX call, cross origin must be allowed.
  [headerFields setObject:@"*" forKey:@"Access-Control-Allow-Origin"];
  base::scoped_nsobject<NSHTTPURLResponse> response([[NSHTTPURLResponse alloc]
       initWithURL:_url
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headerFields]);
  [self.client URLProtocol:self
        didReceiveResponse:response
        cacheStoragePolicy:NSURLCacheStorageNotAllowed];
  [self.client URLProtocol:self didLoadData:[NSData data]];
  [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading {
  // Nothing to do.
}

@end
