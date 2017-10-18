// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/language_detection_logging_helper.h"
#include "components/translate/core/common/translation_logging_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/language/language_model_factory.h"
#include "ios/chrome/browser/language/url_language_histogram_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/translate/after_translate_infobar_controller.h"
#import "ios/chrome/browser/translate/before_translate_infobar_controller.h"
#import "ios/chrome/browser/translate/never_translate_infobar_controller.h"
#include "ios/chrome/browser/translate/translate_accept_languages_factory.h"
#import "ios/chrome/browser/translate/translate_message_infobar_controller.h"
#include "ios/chrome/browser/translate/translate_ranker_factory.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(ChromeIOSTranslateClient);

ChromeIOSTranslateClient::ChromeIOSTranslateClient(web::WebState* web_state)
    : web::WebStateObserver(web_state),
      translate_manager_(base::MakeUnique<translate::TranslateManager>(
          this,
          translate::TranslateRankerFactory::GetForBrowserState(
              ios::ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState())),
          LanguageModelFactory::GetInstance()->GetForBrowserState(
              ios::ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState())))),
      translate_driver_(web_state,
                        web_state->GetNavigationManager(),
                        translate_manager_.get(),
                        UrlLanguageHistogramFactory::GetForBrowserState(
                            ios::ChromeBrowserState::FromBrowserState(
                                web_state->GetBrowserState()))) {}

ChromeIOSTranslateClient::~ChromeIOSTranslateClient() {
}

// static
std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
  return std::unique_ptr<translate::TranslatePrefs>(
      new translate::TranslatePrefs(prefs, prefs::kAcceptLanguages, nullptr));
}

translate::TranslateManager* ChromeIOSTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

// TranslateClient implementation:

std::unique_ptr<infobars::InfoBar> ChromeIOSTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  translate::TranslateStep step = delegate->translate_step();

  std::unique_ptr<InfoBarIOS> infobar(new InfoBarIOS(std::move(delegate)));
  InfoBarController* controller;
  switch (step) {
    case translate::TRANSLATE_STEP_AFTER_TRANSLATE:
      controller = [[AfterTranslateInfoBarController alloc]
          initWithDelegate:infobar.get()];
      break;
    case translate::TRANSLATE_STEP_BEFORE_TRANSLATE:
      controller = [[BeforeTranslateInfoBarController alloc]
          initWithDelegate:infobar.get()];
      break;
    case translate::TRANSLATE_STEP_NEVER_TRANSLATE:
      controller = [[NeverTranslateInfoBarController alloc]
          initWithDelegate:infobar.get()];
      break;
    case translate::TRANSLATE_STEP_TRANSLATING:
    case translate::TRANSLATE_STEP_TRANSLATE_ERROR:
      controller = [[TranslateMessageInfoBarController alloc]
          initWithDelegate:infobar.get()];
      break;
    default:
      NOTREACHED();
  }
  infobar->SetController(controller);
  return infobar;
}

void ChromeIOSTranslateClient::RecordTranslateEvent(
    const metrics::TranslateEventProto& translate_event) {
  if (!base::FeatureList::IsEnabled(switches::kSyncUserTranslationEvents))
    return;

  syncer::UserEventService* const user_event_service =
      IOSUserEventServiceFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(
              web_state()->GetBrowserState()));
  const auto* const item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();

  // If entry is null, we don't record the page.
  // The navigation entry can be null in situations like download or initial
  // blank page.
  if (item == nullptr)
    return;

  auto specifics = base::MakeUnique<sync_pb::UserEventSpecifics>();
  // We only log the event we care about.
  const bool needs_logging = translate::ConstructTranslateEvent(
      item->GetTimestamp().ToInternalValue(), translate_event, specifics.get());
  if (needs_logging) {
    user_event_service->RecordUserEvent(std::move(specifics));
  }
}

void ChromeIOSTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  DCHECK(web_state());
  if (error_type != translate::TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;

  // Infobar UI.
  translate::TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
      translate_manager_->GetWeakPtr(),
      InfoBarManagerImpl::FromWebState(web_state()),
      web_state()->GetBrowserState()->IsOffTheRecord(), step, source_language,
      target_language, error_type, triggered_from_menu);
}

translate::TranslateDriver* ChromeIOSTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* ChromeIOSTranslateClient::GetPrefs() {
  DCHECK(web_state());
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state()->GetBrowserState());
  return chrome_browser_state->GetOriginalChromeBrowserState()->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::GetTranslatePrefs() {
  DCHECK(web_state());
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state()->GetBrowserState());
  return CreateTranslatePrefs(chrome_browser_state->GetPrefs());
}

translate::TranslateAcceptLanguages*
ChromeIOSTranslateClient::GetTranslateAcceptLanguages() {
  DCHECK(web_state());
  return TranslateAcceptLanguagesFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(
          web_state()->GetBrowserState()));
}

int ChromeIOSTranslateClient::GetInfobarIconID() const {
  return IDR_IOS_INFOBAR_TRANSLATE;
}

void ChromeIOSTranslateClient::RecordLanguageDetectionEvent(
    const translate::LanguageDetectionDetails& details) const {
  if (!base::FeatureList::IsEnabled(switches::kSyncUserLanguageDetectionEvents))
    return;

  syncer::UserEventService* const user_event_service =
      IOSUserEventServiceFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(
              web_state()->GetBrowserState()));
  const auto* const item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();

  // If entry is null, we don't record the page.
  // The navigation entry can be null in situations like download or initial
  // blank page.
  if (item != nullptr &&
      TranslateServiceIOS::IsTranslatableURL(item->GetVirtualURL())) {
    user_event_service->RecordUserEvent(
        translate::ConstructLanguageDetectionEvent(
            item->GetTimestamp().ToInternalValue(), details));
  }
}

bool ChromeIOSTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateServiceIOS::IsTranslatableURL(url);
}

void ChromeIOSTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
  NOTREACHED();
}

void ChromeIOSTranslateClient::WebStateDestroyed(web::WebState* web_state) {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with nullptr WebState.
  translate_manager_.reset();
}
