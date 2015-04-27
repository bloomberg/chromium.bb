// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_window_id_manager.h"

#import "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/random.h"

namespace {
// Number of random bytes in unique key for window ID. The length of the
// window ID will be twice this number, as it is hexadecimal encoded.
const NSInteger kUniqueKeyLength = 16;
}  // namespace

@interface CRWJSWindowIdManager () {
  base::scoped_nsobject<NSString> _windowId;
}

// Returns a string of randomized ASCII characters.
- (NSString*)generateUniqueKey;

@end

@implementation CRWJSWindowIdManager

- (id)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  self = [super initWithReceiver:receiver];
  if (self) {
    _windowId.reset([[self generateUniqueKey] retain]);
  }
  return self;
}

- (NSString*)windowId {
  return _windowId;
}

- (void)setWindowId:(NSString*)value {
  _windowId.reset([value copy]);
}

#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"window_id";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.windowIdObject";
}

// It is important to recreate the injection content on every injection, because
// it cotains the randomly-generated page ID used for security checks.
- (NSString*)injectionContent {
  _windowId.reset([[self generateUniqueKey] retain]);
  NSString* script = [super injectionContent];
  return [script stringByReplacingOccurrencesOfString:@"$(WINDOW_ID)"
                                           withString:_windowId];
}

#pragma mark - Private

- (NSString*)generateUniqueKey {
  char randomBytes[kUniqueKeyLength];
  crypto::RandBytes(randomBytes, kUniqueKeyLength);
  return
      base::SysUTF8ToNSString(base::HexEncode(randomBytes, kUniqueKeyLength));
}

@end
