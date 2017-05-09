// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#import "ios/web_view/internal/translate/cwv_language_detection_result_internal.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#import "ios/web_view/public/cwv_translation_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSErrorDomain const CWVTranslationErrorDomain =
    @"org.chromium.chromewebview.TranslationErrorDomain";

const NSInteger CWVTranslationErrorNetwork =
    translate::TranslateErrors::NETWORK;
const NSInteger CWVTranslationErrorInitializationError =
    translate::TranslateErrors::INITIALIZATION_ERROR;
const NSInteger CWVTranslationErrorUnknownLanguage =
    translate::TranslateErrors::UNKNOWN_LANGUAGE;
const NSInteger CWVTranslationErrorUnsupportedLanguage =
    translate::TranslateErrors::UNSUPPORTED_LANGUAGE;
const NSInteger CWVTranslationErrorIdenticalLanguages =
    translate::TranslateErrors::IDENTICAL_LANGUAGES;
const NSInteger CWVTranslationErrorTranslationError =
    translate::TranslateErrors::TRANSLATION_ERROR;
const NSInteger CWVTranslationErrorTranslationTimeout =
    translate::TranslateErrors::TRANSLATION_TIMEOUT;
const NSInteger CWVTranslationErrorUnexpectedScriptError =
    translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR;
const NSInteger CWVTranslationErrorBadOrigin =
    translate::TranslateErrors::BAD_ORIGIN;
const NSInteger CWVTranslationErrorScriptLoadError =
    translate::TranslateErrors::SCRIPT_LOAD_ERROR;

@interface CWVTranslationController ()

// Convenience method to generate all supported languages from
// |translateUIDelegate|.
- (NSArray<CWVTranslationLanguage*>*)supportedLanguages;

// Convenience method to get a language at |index| from |translateUIDelegate|.
- (CWVTranslationLanguage*)languageAtIndex:(size_t)index;

@end

@implementation CWVTranslationController {
  ios_web_view::WebViewTranslateClient* _translateClient;
  std::unique_ptr<translate::TranslateUIDelegate> _translateUIDelegate;
}

@synthesize delegate = _delegate;
@synthesize webState = _webState;

#pragma mark - Internal Methods

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;

  ios_web_view::WebViewTranslateClient::CreateForWebState(_webState);
  _translateClient =
      ios_web_view::WebViewTranslateClient::FromWebState(_webState);
  _translateClient->set_translation_controller(self);
}

- (void)updateTranslateStep:(translate::TranslateStep)step
             sourceLanguage:(const std::string&)sourceLanguage
             targetLanguage:(const std::string&)targetLanguage
                  errorType:(translate::TranslateErrors::Type)errorType
          triggeredFromMenu:(bool)triggeredFromMenu {
  translate::TranslateManager* manager = _translateClient->translate_manager();
  _translateUIDelegate = base::MakeUnique<translate::TranslateUIDelegate>(
      manager->GetWeakPtr(), sourceLanguage, targetLanguage);

  CWVTranslationLanguage* source =
      [self languageAtIndex:_translateUIDelegate->GetOriginalLanguageIndex()];
  CWVTranslationLanguage* target =
      [self languageAtIndex:_translateUIDelegate->GetTargetLanguageIndex()];

  NSError* error;
  if (errorType != translate::TranslateErrors::NONE) {
    error = [NSError errorWithDomain:CWVTranslationErrorDomain
                                code:errorType
                            userInfo:nil];
  }

  switch (step) {
    case translate::TRANSLATE_STEP_BEFORE_TRANSLATE: {
      CWVLanguageDetectionResult* languageDetectionResult =
          [[CWVLanguageDetectionResult alloc]
                 initWithPageLanguage:source
              suggestedTargetLanguage:target
                   supportedLanguages:[self supportedLanguages]];
      if ([_delegate respondsToSelector:@selector
                     (translationController:didFinishLanguageDetectionWithResult
                                              :error:)]) {
        [_delegate translationController:self
            didFinishLanguageDetectionWithResult:languageDetectionResult
                                           error:error];
      }
      break;
    }
    case translate::TRANSLATE_STEP_TRANSLATING:
      if ([_delegate respondsToSelector:@selector
                     (translationController:didStartTranslationFromLanguage
                                              :toLanguage:)]) {
        [_delegate translationController:self
            didStartTranslationFromLanguage:source
                                 toLanguage:target];
      }
      break;
    case translate::TRANSLATE_STEP_AFTER_TRANSLATE:
      if ([_delegate respondsToSelector:@selector
                     (translationController:didFinishTranslationFromLanguage
                                              :toLanguage:error:)]) {
        [_delegate translationController:self
            didFinishTranslationFromLanguage:source
                                  toLanguage:target
                                       error:error];
      }
      break;
    case translate::TRANSLATE_STEP_NEVER_TRANSLATE:
      break;
    case translate::TRANSLATE_STEP_TRANSLATE_ERROR:
      break;
  }
}

#pragma mark - Public Methods

- (void)translatePageFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                       toLanguage:(CWVTranslationLanguage*)targetLanguage {
  // TODO(706289): Use the passed parameters.
  _translateUIDelegate->Translate();
}

- (void)revertTranslation {
  _translateUIDelegate->RevertTranslation();
}

#pragma mark - Private Methods

- (NSArray<CWVTranslationLanguage*>*)supportedLanguages {
  NSMutableArray* supportedLanguages = [NSMutableArray array];
  for (size_t i = 0; i < _translateUIDelegate->GetNumberOfLanguages(); i++) {
    CWVTranslationLanguage* language = [self languageAtIndex:i];
    [supportedLanguages addObject:language];
  }

  return [supportedLanguages copy];
}

- (CWVTranslationLanguage*)languageAtIndex:(size_t)index {
  std::string languageCode = _translateUIDelegate->GetLanguageCodeAt(index);
  base::string16 languageName = _translateUIDelegate->GetLanguageNameAt(index);
  return [[CWVTranslationLanguage alloc] initWithLanguageCode:languageCode
                                                 languageName:languageName];
}

@end
