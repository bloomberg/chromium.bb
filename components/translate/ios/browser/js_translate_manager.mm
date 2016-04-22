// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/scoped_nsobject.h"

@implementation JsTranslateManager {
  base::scoped_nsobject<NSString> _translationScript;
}

- (NSString*)script {
  return _translationScript.get();
}

- (void)setScript:(NSString*)script {
  // The translation script uses performance.now() for metrics, which is not
  // supported except on iOS 8.0. To make the translation script work on these
  // iOS versions, add some JavaScript to |script| that defines an
  // implementation of performance.now().
  NSString* const kPerformancePlaceholder =
      @"var performance = window['performance'] || {};"
      @"performance.now = performance['now'] ||"
      @"(function () { return Date.now(); });\n";
  script = [kPerformancePlaceholder stringByAppendingString:script];
  // TODO(shreyasv): This leads to some duplicate code from
  // CRWJSInjectionManager. Consider refactoring this to its own js injection
  // manager.
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:@"translate_ios"
                                             ofType:@"js"];
  DCHECK(path);
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error && [content length]);
  script = [script stringByAppendingString:content];
  _translationScript.reset([script copy]);
}

- (void)injectWaitUntilTranslateReadyScript {
  [self.receiver evaluateJavaScript:@"__gCrWeb.translate.checkTranslateReady()"
                stringResultHandler:nil];
}

- (void)injectTranslateStatusScript {
  [self.receiver evaluateJavaScript:@"__gCrWeb.translate.checkTranslateStatus()"
                stringResultHandler:nil];
}

- (void)startTranslationFrom:(const std::string&)source
                          to:(const std::string&)target {
  NSString* js =
      [NSString stringWithFormat:@"cr.googleTranslate.translate('%s','%s')",
                                 source.c_str(), target.c_str()];
  [self.receiver evaluateJavaScript:js stringResultHandler:nil];
}

- (void)revertTranslation {
  DCHECK([self hasBeenInjected]);
  [self.receiver evaluateJavaScript:@"cr.googleTranslate.revert()"
                stringResultHandler:nil];
}

#pragma mark -
#pragma mark CRWJSInjectionManager methods

- (NSString*)injectionContent {
  DCHECK(_translationScript);
  return _translationScript.autorelease();
}

- (NSString*)presenceBeacon {
  return @"cr.googleTranslate";
}

@end
