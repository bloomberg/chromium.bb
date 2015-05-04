// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/clients/crw_csp_network_client.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/web/net/crw_url_verifying_protocol_handler.h"

namespace {

// HTTP headers for the content security policy.
NSString* const kCSPHeaders[] {
  @"Content-Security-Policy", @"Content-Security-Policy-Report-Only",
      @"X-WebKit-CSP", @"X-WebKit-CSP-Report-Only"
};

NSString* const kDefaultSrc = @"default-src";
NSString* const kConnectSrc = @"connect-src";
NSString* const kSelf = @"'self'";
NSString* const kFrameSrc = @"frame-src";
NSString* const kFrameValue = @" crwebinvoke: crwebinvokeimmediate: crwebnull:";

// Value of the 'connect-src' directive for the Content Security Policy.
// Lazily initialized.
NSString* g_connect_value = nil;

// Adds |value| (i.e. 'self') to the CSP |directive| (i.e. 'frame-src').
// |header| is the value of the 'Content-Security-Policy' header and is modified
// by the function.
// If |directive| is not in the CSP, the function checks for 'default-src' and
// adds |value| there if needed.
void RelaxCspValue(NSString* directive,
                            NSString* value,
                            NSMutableString* header) {
  DCHECK(directive);
  DCHECK(value);
  DCHECK(header);
  // The function is sub-optimal if the directive is 'default-src' as we could
  // skip one of the calls to |-rangeOfString:options:| in that case.
  // Please consider improving the implementation if you need to support this.
  DCHECK(![directive isEqualToString:kDefaultSrc]);

  // If |directive| is already present in |header|, |value| is prepended to the
  // existing value.
  NSRange range =
      [header rangeOfString:directive options:NSCaseInsensitiveSearch];
  if (range.location == NSNotFound) {
    // Else, if the 'default-src' directive is present, |value| is prepended to
    // the existing value of "default-src".
    range = [header rangeOfString:kDefaultSrc options:NSCaseInsensitiveSearch];
  }

  if (range.location != NSNotFound) {
    [header insertString:value atIndex:NSMaxRange(range)];
    return;
  }

  // Else, there is no |directive| and no 'default-src', nothing to do.
}

}  // namespace

@implementation CRWCspNetworkClient

- (void)didReceiveResponse:(NSURLResponse*)response {
  if (![response isKindOfClass:[NSHTTPURLResponse class]]) {
    [super didReceiveResponse:response];
    return;
  }

  NSHTTPURLResponse* httpResponse = static_cast<NSHTTPURLResponse*>(response);
  base::scoped_nsobject<NSDictionary> inputHeaders(
      [[httpResponse allHeaderFields] retain]);

  // Enumerate the headers and return early if there is nothing to do.
  bool hasCspHeader = false;
  for (NSString* key in inputHeaders.get()) {
    for (size_t i = 0; i < arraysize(kCSPHeaders); ++i) {
      if ([key caseInsensitiveCompare:kCSPHeaders[i]] == NSOrderedSame) {
        hasCspHeader = true;
        break;
      }
    }
    if (hasCspHeader)
      break;
  }

  if (!hasCspHeader) {
    // No CSP header, return early.
    [super didReceiveResponse:response];
    return;
  }

  if (!g_connect_value) {
    g_connect_value = [[NSString alloc]
        initWithFormat:@" %@ %s", kSelf, web::kURLForVerification];
  }

  base::scoped_nsobject<NSMutableDictionary> outputHeaders(
      [[NSMutableDictionary alloc] init]);

  // Add some values to the content security policy headers in order to keep the
  // URL verification and the javascript injection working.
  for (NSString* key in inputHeaders.get()) {
    base::scoped_nsobject<NSString> header(
        [[inputHeaders objectForKey:key] retain]);
    for (size_t i = 0; i < arraysize(kCSPHeaders); ++i) {
      if ([key caseInsensitiveCompare:kCSPHeaders[i]] != NSOrderedSame)
        continue;
      base::scoped_nsobject<NSMutableString> cspHeader(
          [[NSMutableString alloc] initWithString:header]);
      // Fix connect-src.
      RelaxCspValue(kConnectSrc, g_connect_value, cspHeader);
      // Fix frame-src.
      RelaxCspValue(kFrameSrc, kFrameValue, cspHeader);
      header.reset([cspHeader retain]);
      break;
    }
    DCHECK(![outputHeaders objectForKey:key]);
    [outputHeaders setObject:header forKey:key];
  }

  // Build a new response with |outputHeaders|.
  base::scoped_nsobject<NSHTTPURLResponse> outResponse(
      [[NSHTTPURLResponse alloc] initWithURL:[httpResponse URL]
                                  statusCode:[httpResponse statusCode]
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:outputHeaders]);
  [super didReceiveResponse:outResponse];
}

@end
