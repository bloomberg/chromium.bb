// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_language_detection_manager.h"

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_state/js/crw_js_early_script_manager.h"

namespace language_detection {
// Note: This should stay in sync with the constant in language_detection.js.
const size_t kMaxIndexChars = 65535;
}  // namespace language_detection

@implementation JsLanguageDetectionManager

#pragma mark - Protected methods

- (NSString*)scriptPath {
  return @"language_detection";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.languageDetection";
}

- (NSArray*)directDependencies {
  return @[ [CRWJSEarlyScriptManager class] ];
}

#pragma mark - Public methods

- (void)startLanguageDetection {
  [self evaluate:@"__gCrWeb.languageDetection.detectLanguage()"
      stringResultHandler:nil];
}

- (void)retrieveBufferedTextContent:
        (const language_detection::BufferedTextCallback&)callback {
  DCHECK(!callback.is_null());
  // Copy the completion handler so that the block does not capture a reference.
  __block language_detection::BufferedTextCallback blockCallback = callback;
  [self evaluate:@"__gCrWeb.languageDetection.retrieveBufferedTextContent()"
      stringResultHandler:^(NSString* result, NSError*) {
        blockCallback.Run(base::SysNSStringToUTF16(result));
      }];
}

@end
