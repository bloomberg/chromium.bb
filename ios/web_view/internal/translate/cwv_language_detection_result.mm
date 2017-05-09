// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_language_detection_result_internal.h"

#import "ios/web_view/public/cwv_translation_language.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVLanguageDetectionResult

@synthesize pageLanguage = _pageLanguage;
@synthesize suggestedTargetLanguage = _suggestedTargetLanguage;
@synthesize supportedLanguages = _supportedLanguages;

- (instancetype)
   initWithPageLanguage:(CWVTranslationLanguage*)pageLanguage
suggestedTargetLanguage:(CWVTranslationLanguage*)suggestedTargetLanguage
     supportedLanguages:(NSArray<CWVTranslationLanguage*>*)supportedLanguages {
  self = [super init];
  if (self) {
    _pageLanguage = pageLanguage;
    _suggestedTargetLanguage = suggestedTargetLanguage;
    _supportedLanguages = supportedLanguages;
  }
  return self;
}

@end
