// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_LANGUAGE_DETECTION_RESULT_H
#define IOS_WEB_VIEW_PUBLIC_CWV_LANGUAGE_DETECTION_RESULT_H

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationLanguage;

// Encapsulates the results of language detection in one class.
CWV_EXPORT
@interface CWVLanguageDetectionResult : NSObject

- (instancetype)init NS_UNAVAILABLE;

// The detected language of the page.
@property(nonatomic, readonly) CWVTranslationLanguage* pageLanguage;

// The suggested language to translate to.
@property(nonatomic, readonly) CWVTranslationLanguage* suggestedTargetLanguage;

// The list of supported languages that can be used in translation.
@property(nonatomic, copy, readonly)
    NSArray<CWVTranslationLanguage*>* supportedLanguages;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_LANGUAGE_DETECTION_RESULT_H
