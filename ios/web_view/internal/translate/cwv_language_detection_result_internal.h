// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_LANGUAGE_DETECTION_RESULT_INTERNAL_H
#define IOS_WEB_VIEW_INTERNAL_CWV_LANGUAGE_DETECTION_RESULT_INTERNAL_H

#import <Foundation/Foundation.h>

#import "ios/web_view/public/cwv_language_detection_result.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationLanguage;

// Internal methods for CWVLanguageDetectionResult.
@interface CWVLanguageDetectionResult ()

- (instancetype)
   initWithPageLanguage:(CWVTranslationLanguage*)pageLanguage
suggestedTargetLanguage:(CWVTranslationLanguage*)suggestedTargetLanguage
     supportedLanguages:(NSArray<CWVTranslationLanguage*>*)supportedLanguages
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_LANGUAGE_DETECTION_RESULT_INTERNAL_H
