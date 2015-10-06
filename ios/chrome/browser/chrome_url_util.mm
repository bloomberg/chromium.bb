// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_url_util.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/net/url_scheme_util.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"
#include "url/url_constants.h"

bool UrlIsExternalFileReference(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) &&
         base::LowerCaseEqualsASCII(url.host(), kChromeUIExternalFileHost);
}

NSURL* UrlToLaunchChrome() {
  // Determines the target URL scheme that will launch Chrome.
  NSString* scheme = [[ChromeAppConstants sharedInstance] getBundleURLScheme];
  return [NSURL URLWithString:[NSString stringWithFormat:@"%@://", scheme]];
}

NSURL* UrlOfChromeAppIcon(int width, int height) {
  NSString* url =
      [[ChromeAppConstants sharedInstance] getChromeAppIconURLOfWidth:width
                                                               height:height];
  return [NSURL URLWithString:url];
}

bool UrlHasChromeScheme(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme);
}

bool UrlHasChromeScheme(NSURL* url) {
  return net::UrlSchemeIs(url, base::SysUTF8ToNSString(kChromeUIScheme));
}

bool IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  if (scheme == url::kAboutScheme)
    return true;
  if (scheme == url::kDataScheme)
    return true;
  if (scheme == kChromeUIScheme)
    return true;
  return net::URLRequest::IsHandledProtocol(scheme);
}

@implementation ChromeAppConstants {
  base::scoped_nsobject<NSString> _callbackScheme;
  base::mac::ScopedBlock<AppIconURLProvider> _appIconURLProvider;
}

+ (ChromeAppConstants*)sharedInstance {
  static ChromeAppConstants* g_instance = [[ChromeAppConstants alloc] init];
  return g_instance;
}

- (NSString*)getBundleURLScheme {
  if (!_callbackScheme) {
    NSSet* allowableSchemes =
        [NSSet setWithObjects:@"googlechrome", @"chromium", nil];
    NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
    NSArray* urlTypes = [info objectForKey:@"CFBundleURLTypes"];
    for (NSDictionary* urlType in urlTypes) {
      DCHECK([urlType isKindOfClass:[NSDictionary class]]);
      NSArray* schemes = [urlType objectForKey:@"CFBundleURLSchemes"];
      if (!schemes)
        continue;
      DCHECK([schemes isKindOfClass:[NSArray class]]);
      for (NSString* scheme in schemes) {
        if ([allowableSchemes containsObject:scheme])
          _callbackScheme.reset([scheme copy]);
      }
    }
  }
  DCHECK([_callbackScheme length]);
  return _callbackScheme;
}

- (NSString*)getChromeAppIconURLOfWidth:(int)width height:(int)height {
  if (!_appIconURLProvider) {
    // TODO(ios): iOS code should default to "Chromium" branding.
    // http://crbug.com/489270
    _appIconURLProvider.reset(^(int width, int height) {
      return [NSString
          stringWithFormat:@"https://%@/Icon-%dx%d-%dx.png",
                           @"ssl.gstatic.com/ios-apps/com.google.chrome.ios",
                           width, height,
                           [[UIScreen mainScreen] scale] == 1.0 ? 1 : 2];
    }, base::scoped_policy::RETAIN);
  }
  return _appIconURLProvider.get()(width, height);
}

- (void)setCallbackSchemeForTesting:(NSString*)scheme {
  _callbackScheme.reset([scheme copy]);
}

- (void)setAppIconURLProviderForTesting:(AppIconURLProvider)block {
  _appIconURLProvider.reset(block, base::scoped_policy::RETAIN);
}

@end
