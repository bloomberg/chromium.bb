// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "cwv_export.h"

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_DELEGATE_H
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_DELEGATE_H

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationController;
@class CWVLanguageDetectionResult;
@class CWVTranslationLanguage;

// Updates delegate on translation progress.
CWV_EXPORT
@protocol CWVTranslationControllerDelegate<NSObject>

@required
// Called when language detection is completed. Contains information on the
// detected language, the assumed target language for translation, as well as
// all languages supported by this translation controller.
// |error| will be nonnull if language detection failed.
- (void)translationController:(CWVTranslationController*)controller
    didFinishLanguageDetectionWithResult:(CWVLanguageDetectionResult*)result
                                   error:(nullable NSError*)error;

@optional
// Called when a translation has started.
- (void)translationController:(CWVTranslationController*)controller
    didStartTranslationFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                         toLanguage:(CWVTranslationLanguage*)targetLanguage;

// Called when translation finishes. |error| will be nonnull if it failed.
- (void)translationController:(CWVTranslationController*)controller
    didFinishTranslationFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                          toLanguage:(CWVTranslationLanguage*)targetLanguage
                               error:(nullable NSError*)error;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_DELEGATE_H
