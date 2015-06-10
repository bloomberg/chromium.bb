// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_crash_detector.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#import "base/strings/sys_string_conversions.h"

namespace {

// WKWebView property key path that is changed when web process is terminated.
// The value of this property is not important, but its change may indicate
// web process termination.
NSString* const kCrashIndicatorKeyPath = @"scrollView.backgroundColor";

// Returns |YES| if given |error| means that WKWebView process was terminated.
BOOL IsWebViewTerminationError(NSError* error) {
  DLOG_IF(WARNING, ![error.domain isEqualToString:WKErrorDomain])
      << "Unexpected WKWebView error domain: "
      << base::SysNSStringToUTF8(error.domain);
  // WKErrorUnknown is returned from unusable WKWebView so assume it's crashed.
  return ((error.code == WKErrorWebContentProcessTerminated ||
           error.code == WKErrorUnknown) &&
          [error.domain isEqualToString:WKErrorDomain]);
}

}  // namespace

@interface CRWWKWebViewCrashDetector ()

// Checks WKWebView health by evaluating a simple JavaScript. If evaluation
// fails with Termination Error then crash is reported.
- (void)checkHealth;

// Calls _crashHandler.
- (void)reportCrash;

@end

@implementation CRWWKWebViewCrashDetector {
  base::scoped_nsobject<WKWebView> _webView;
  base::mac::ScopedBlock<ProceduralBlock> _crashHandler;
}

- (instancetype)initWithWebView:(WKWebView*)webView
                   crashHandler:(ProceduralBlock)handler {
  DCHECK(webView);
  DCHECK(handler);

  if (self = [super init]) {
    _webView.reset([webView retain]);
    _crashHandler.reset([handler copy]);

    // Once Web Process is terminated -[WKWebView _processDidExit] is called.
    // That private method does different things inside including the change
    // of scroll view's background color. We use that color change as an
    // indication that WKWebView process may have crashed.
    [_webView addObserver:self
               forKeyPath:kCrashIndicatorKeyPath
                  options:0
                  context:nil];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:kCrashIndicatorKeyPath];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([keyPath isEqualToString:kCrashIndicatorKeyPath]);
  [self checkHealth];
}

- (void)checkHealth {
  base::WeakNSObject<CRWWKWebViewCrashDetector> weakSelf(self);
  [_webView evaluateJavaScript:@"null"
             completionHandler:^(id, NSError* error) {
               if (error && IsWebViewTerminationError(error))
                 [weakSelf reportCrash];
             }];
}

- (void)reportCrash {
  _crashHandler.get()();
}

@end
