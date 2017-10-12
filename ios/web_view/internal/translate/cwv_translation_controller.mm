// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#import "ios/web_view/public/cwv_translation_controller_delegate.h"
#import "ios/web_view/public/cwv_translation_policy.h"
#include "ui/base/l10n/l10n_util.h"

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

// A map of CWTranslationLanguages keyed by its language code. Lazily loaded.
@property(nonatomic, readonly)
    NSDictionary<NSString*, CWVTranslationLanguage*>* supportedLanguagesByCode;

// Convenience method to get the language with |languageCode|.
- (CWVTranslationLanguage*)languageWithCode:(const std::string&)languageCode;

@end

@implementation CWVTranslationController {
  ios_web_view::WebViewTranslateClient* _translateClient;
  std::unique_ptr<translate::TranslatePrefs> _translatePrefs;
}

@synthesize delegate = _delegate;
@synthesize supportedLanguagesByCode = _supportedLanguagesByCode;
@synthesize webState = _webState;

#pragma mark - Internal Methods

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;

  ios_web_view::WebViewTranslateClient::CreateForWebState(_webState);
  _translateClient =
      ios_web_view::WebViewTranslateClient::FromWebState(_webState);
  _translateClient->set_translation_controller(self);
  _translatePrefs = _translateClient->translate_manager()
                        ->translate_client()
                        ->GetTranslatePrefs();
}

- (void)updateTranslateStep:(translate::TranslateStep)step
             sourceLanguage:(const std::string&)sourceLanguage
             targetLanguage:(const std::string&)targetLanguage
                  errorType:(translate::TranslateErrors::Type)errorType
          triggeredFromMenu:(bool)triggeredFromMenu {
  CWVTranslationLanguage* source = [self languageWithCode:sourceLanguage];
  CWVTranslationLanguage* target = [self languageWithCode:targetLanguage];

  NSError* error;
  if (errorType != translate::TranslateErrors::NONE) {
    error = [NSError errorWithDomain:CWVTranslationErrorDomain
                                code:errorType
                            userInfo:nil];
  }

  switch (step) {
    case translate::TRANSLATE_STEP_BEFORE_TRANSLATE: {
      if ([_delegate respondsToSelector:@selector
                     (translationController:canOfferTranslationFromLanguage
                                              :toLanguage:)]) {
        [_delegate translationController:self
            canOfferTranslationFromLanguage:source
                                 toLanguage:target];
      }
      break;
    }
    case translate::TRANSLATE_STEP_TRANSLATING:
      if ([_delegate respondsToSelector:@selector
                     (translationController:didStartTranslationFromLanguage
                                              :toLanguage:userInitiated:)]) {
        [_delegate translationController:self
            didStartTranslationFromLanguage:source
                                 toLanguage:target
                              userInitiated:triggeredFromMenu];
      }
      break;
    case translate::TRANSLATE_STEP_AFTER_TRANSLATE:
    case translate::TRANSLATE_STEP_TRANSLATE_ERROR:
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
      // Not supported.
      break;
  }
}

#pragma mark - Public Methods

- (NSSet*)supportedLanguages {
  return [NSSet setWithArray:self.supportedLanguagesByCode.allValues];
}

- (void)translatePageFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                       toLanguage:(CWVTranslationLanguage*)targetLanguage
                    userInitiated:(BOOL)userInitiated {
  std::string sourceLanguageCode =
      base::SysNSStringToUTF8(sourceLanguage.languageCode);
  std::string targetLanguageCode =
      base::SysNSStringToUTF8(targetLanguage.languageCode);
  _translateClient->translate_manager()->TranslatePage(
      sourceLanguageCode, targetLanguageCode, userInitiated);
}

- (void)revertTranslation {
  _translateClient->translate_manager()->RevertTranslation();
}

- (void)setTranslationPolicy:(CWVTranslationPolicy*)policy
             forPageLanguage:(CWVTranslationLanguage*)pageLanguage {
  std::string languageCode = base::SysNSStringToUTF8(pageLanguage.languageCode);
  switch (policy.type) {
    case CWVTranslationPolicyAsk: {
      _translatePrefs->UnblockLanguage(languageCode);
      _translatePrefs->RemoveLanguagePairFromWhitelist(languageCode,
                                                       std::string());
      break;
    }
    case CWVTranslationPolicyNever: {
      _translatePrefs->AddToLanguageList(languageCode,
                                         /*force_blocked=*/true);
      break;
    }
    case CWVTranslationPolicyAuto: {
      _translatePrefs->UnblockLanguage(languageCode);
      _translatePrefs->WhitelistLanguagePair(
          languageCode, base::SysNSStringToUTF8(policy.language.languageCode));
      break;
    }
  }
}

- (CWVTranslationPolicy*)translationPolicyForPageLanguage:
    (CWVTranslationLanguage*)pageLanguage {
  std::string languageCode = base::SysNSStringToUTF8(pageLanguage.languageCode);
  bool isLanguageBlocked = _translatePrefs->IsBlockedLanguage(languageCode);
  if (isLanguageBlocked) {
    return [CWVTranslationPolicy translationPolicyNever];
  }

  std::string autoTargetLanguageCode;
  if (!_translatePrefs->ShouldAutoTranslate(languageCode,
                                            &autoTargetLanguageCode)) {
    return [CWVTranslationPolicy translationPolicyAsk];
  }

  CWVTranslationLanguage* targetLanguage =
      [self languageWithCode:autoTargetLanguageCode];
  return [CWVTranslationPolicy
      translationPolicyAutoTranslateToLanguage:targetLanguage];
}

- (void)setTranslationPolicy:(CWVTranslationPolicy*)policy
                 forPageHost:(NSString*)pageHost {
  DCHECK(pageHost.length);
  switch (policy.type) {
    case CWVTranslationPolicyAsk: {
      _translatePrefs->RemoveSiteFromBlacklist(
          base::SysNSStringToUTF8(pageHost));
      break;
    }
    case CWVTranslationPolicyNever: {
      _translatePrefs->BlacklistSite(base::SysNSStringToUTF8(pageHost));
      break;
    }
    case CWVTranslationPolicyAuto: {
      // TODO(crbug.com/706289): Support auto translation policies for websites.
      NOTREACHED();
      break;
    }
  }
}

- (CWVTranslationPolicy*)translationPolicyForPageHost:(NSString*)pageHost {
  // TODO(crbug.com/706289): Return translationPolicyAuto when implemented.
  bool isSiteBlackListed =
      _translatePrefs->IsSiteBlacklisted(base::SysNSStringToUTF8(pageHost));
  if (isSiteBlackListed) {
    return [CWVTranslationPolicy translationPolicyNever];
  }
  return [CWVTranslationPolicy translationPolicyAsk];
}

#pragma mark - Private Methods

- (NSDictionary<NSString*, CWVTranslationLanguage*>*)supportedLanguagesByCode {
  if (!_supportedLanguagesByCode) {
    NSMutableDictionary<NSString*, CWVTranslationLanguage*>*
        supportedLanguagesByCode = [NSMutableDictionary dictionary];
    std::vector<std::string> languageCodes;
    translate::TranslateDownloadManager::GetSupportedLanguages(&languageCodes);
    std::string locale = translate::TranslateDownloadManager::GetInstance()
                             ->application_locale();
    for (const std::string& languageCode : languageCodes) {
      base::string16 localizedName =
          l10n_util::GetDisplayNameForLocale(languageCode, locale, true);
      base::string16 nativeName =
          l10n_util::GetDisplayNameForLocale(languageCode, languageCode, true);
      CWVTranslationLanguage* language =
          [[CWVTranslationLanguage alloc] initWithLanguageCode:languageCode
                                                 localizedName:localizedName
                                                    nativeName:nativeName];

      supportedLanguagesByCode[language.languageCode] = language;
    }

    _supportedLanguagesByCode = [supportedLanguagesByCode copy];
  }
  return _supportedLanguagesByCode;
}

- (CWVTranslationLanguage*)languageWithCode:(const std::string&)languageCode {
  NSString* languageCodeString = base::SysUTF8ToNSString(languageCode);
  return self.supportedLanguagesByCode[languageCodeString];
}

@end
