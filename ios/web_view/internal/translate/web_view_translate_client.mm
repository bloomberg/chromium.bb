// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/web_view_translate_client.h"

#include <vector>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/language_detection_details.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/pref_names.h"
#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"
#include "ios/web_view/internal/translate/web_view_translate_accept_languages_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(ios_web_view::WebViewTranslateClient);

namespace ios_web_view {

namespace {
// Translate settings for off-the-record browser states are inherited from a
// non-off-the-record browser state. This allows them to share settings.
WebViewBrowserState* GetMainBrowserState() {
  return [CWVWebViewConfiguration defaultConfiguration].browserState;
}
}  // namespace

// TODO(crbug.com/729859): Support logging histogram data on detected language
// page, by passing a valid language_histogram, when histogram logging is
// available on ios/web_view.
WebViewTranslateClient::WebViewTranslateClient(web::WebState* web_state)
    : web::WebStateObserver(web_state),
      translate_manager_(base::MakeUnique<translate::TranslateManager>(
          this,
          WebViewTranslateRankerFactory::GetForBrowserState(
              WebViewBrowserState::FromBrowserState(GetMainBrowserState())),
          nullptr /* language_model */)),
      translate_driver_(web_state,
                        web_state->GetNavigationManager(),
                        translate_manager_.get(),
                        nullptr /* language_histogram */) {}

WebViewTranslateClient::~WebViewTranslateClient() = default;

// TranslateClient implementation:

std::unique_ptr<infobars::InfoBar> WebViewTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  NOTREACHED();
  return nullptr;
}

void WebViewTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  [translation_controller_ updateTranslateStep:step
                                sourceLanguage:source_language
                                targetLanguage:target_language
                                     errorType:error_type
                             triggeredFromMenu:triggered_from_menu];
}

translate::TranslateDriver* WebViewTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* WebViewTranslateClient::GetPrefs() {
  return GetMainBrowserState()->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
WebViewTranslateClient::GetTranslatePrefs() {
  DCHECK(web_state());
  return base::MakeUnique<translate::TranslatePrefs>(
      GetPrefs(), prefs::kAcceptLanguages, nullptr);
}

translate::TranslateAcceptLanguages*
WebViewTranslateClient::GetTranslateAcceptLanguages() {
  translate::TranslateAcceptLanguages* accept_languages =
      WebViewTranslateAcceptLanguagesFactory::GetForBrowserState(
          WebViewBrowserState::FromBrowserState(GetMainBrowserState()));
  DCHECK(accept_languages);
  return accept_languages;
}

int WebViewTranslateClient::GetInfobarIconID() const {
  NOTREACHED();
  return 0;
}

void WebViewTranslateClient::RecordLanguageDetectionEvent(
    const translate::LanguageDetectionDetails& details) const {
  // TODO(crbug.com/722679): Implementing gaia-keyed logging.
}

void WebViewTranslateClient::RecordTranslateEvent(
    const metrics::TranslateEventProto&) {
  // TODO(crbug.com/728491): Implementing gaia-keyed logging.
}

bool WebViewTranslateClient::IsTranslatableURL(const GURL& url) {
  return !url.is_empty() && !url.SchemeIs(url::kFtpScheme);
}

void WebViewTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
  NOTREACHED();
}

void WebViewTranslateClient::WebStateDestroyed(web::WebState* web_state) {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with nullptr WebState.
  translate_manager_.reset();
}

}  // namespace ios_web_view
