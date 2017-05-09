// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_H
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_H

#import <ChromeWebView/cwv_export.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVTranslationLanguage;
@protocol CWVTranslationControllerDelegate;

// The error domain for translation errors.
extern NSErrorDomain const CWVTranslationErrorDomain;

// Possible error codes during translation.
// No connectivity.
extern const NSInteger CWVTranslationErrorNetwork;
// The translation script failed to initialize.
extern const NSInteger CWVTranslationErrorInitializationError;
// The page's language could not be detected.
extern const NSInteger CWVTranslationErrorUnknownLanguage;
// The server detected a language that the browser does not know.
extern const NSInteger CWVTranslationErrorUnsupportedLanguage;
// The original and target languages are the same.
extern const NSInteger CWVTranslationErrorIdenticalLanguages;
// An error was reported by the translation script during translation.
extern const NSInteger CWVTranslationErrorTranslationError;
// The library doesn't finish the translation.
extern const NSInteger CWVTranslationErrorTranslationTimeout;
// The library raises an unexpected exception.
extern const NSInteger CWVTranslationErrorUnexpectedScriptError;
// The library is blocked because of bad origin.
extern const NSInteger CWVTranslationErrorBadOrigin;
// Loader fails to load a dependent JavaScript.
extern const NSInteger CWVTranslationErrorScriptLoadError;

// Allows page translation from one language to another.
CWV_EXPORT
@interface CWVTranslationController : NSObject

// Delegate to receive translation callbacks.
@property(nullable, nonatomic, weak) id<CWVTranslationControllerDelegate>
    delegate;

// Begins translation on the current page from |sourceLanguage| to
// |targetLanguage|. These language parameters must be chosen from
// a CWWLanguageDetectionResult's |supportedLanguages|.
// This must not be called before the |delegate| receives
// |translationController:didFinishLanguageDetectionWithResult:error:|.
// TODO(crbug.com/706289): Document what happens if you call this out of order
// or many times.
- (void)translatePageFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                       toLanguage:(CWVTranslationLanguage*)targetLanguage;

// Reverts any translations done back to the original page language.
// Note that the original page language  may be different from |sourceLanguage|
// passed to |translatePageFromLanguage:toLanguage:| above.
// This must not be called before the |delegate| receives
// |translationController:didFinishLanguageDetectionWithResult:error:|.
// TODO(crbug.com/706289): Document what happens if you call this out of order.
- (void)revertTranslation;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATION_CONTROLLER_H
