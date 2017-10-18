// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/ios/browser/language_detection_controller.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/prefs/pref_member.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#import "components/translate/ios/browser/js_language_detection_manager.h"
#include "components/translate/ios/browser/string_clipping_util.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/web_state.h"
#include "net/http/http_response_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

namespace {
// Name for the UMA metric used to track text extraction time.
const char kTranslateCaptureText[] = "Translate.CaptureText";
// Prefix for the language detection javascript commands. Must be kept in sync
// with language_detection.js.
const char kCommandPrefix[] = "languageDetection";
}

LanguageDetectionController::LanguageDetectionController(
    web::WebState* web_state,
    JsLanguageDetectionManager* manager,
    PrefService* prefs)
    : web::WebStateObserver(web_state),
      js_manager_(manager),
      weak_method_factory_(this) {
  DCHECK(web::WebStateObserver::web_state());
  DCHECK(js_manager_);
  translate_enabled_.Init(prefs::kEnableTranslate, prefs);
  web_state->AddScriptCommandCallback(
      base::Bind(&LanguageDetectionController::OnTextCaptured,
                 base::Unretained(this)),
      kCommandPrefix);
}

LanguageDetectionController::~LanguageDetectionController() {
}

LanguageDetectionController::DetectionDetails::DetectionDetails()
    : is_cld_reliable(false) {}

LanguageDetectionController::DetectionDetails::DetectionDetails(
    const DetectionDetails& other) = default;

LanguageDetectionController::DetectionDetails::~DetectionDetails() = default;

std::unique_ptr<LanguageDetectionController::CallbackList::Subscription>
LanguageDetectionController::RegisterLanguageDetectionCallback(
    const Callback& callback) {
  return language_detection_callbacks_.Add(callback);
}

void LanguageDetectionController::StartLanguageDetection() {
  if (!translate_enabled_.GetValue())
    return;  // Translate disabled in preferences.
  DCHECK(web_state());
  const GURL& url = web_state()->GetVisibleURL();
  if (!web::UrlHasWebScheme(url) || !web_state()->ContentIsHTML())
    return;
  [js_manager_ inject];
  [js_manager_ startLanguageDetection];
}

bool LanguageDetectionController::OnTextCaptured(
    const base::DictionaryValue& command,
    const GURL& url,
    bool interacting) {
  std::string textCapturedCommand;
  if (!command.GetString("command", &textCapturedCommand) ||
      textCapturedCommand != "languageDetection.textCaptured" ||
      !command.HasKey("translationAllowed")) {
    NOTREACHED();
    return false;
  }
  bool translation_allowed = false;
  command.GetBoolean("translationAllowed", &translation_allowed);
  if (!translation_allowed) {
    // Translation not allowed by the page. Done processing.
    return true;
  }
  if (!command.HasKey("captureTextTime") || !command.HasKey("htmlLang") ||
      !command.HasKey("httpContentLanguage")) {
    NOTREACHED();
    return false;
  }

  double capture_text_time = 0;
  command.GetDouble("captureTextTime", &capture_text_time);
  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeDelta::FromMillisecondsD(capture_text_time));
  std::string html_lang;
  command.GetString("htmlLang", &html_lang);
  std::string http_content_language;
  command.GetString("httpContentLanguage", &http_content_language);
  // If there is no language defined in httpEquiv, use the HTTP header.
  if (http_content_language.empty())
    http_content_language = content_language_header_;

  [js_manager_ retrieveBufferedTextContent:
                   base::Bind(&LanguageDetectionController::OnTextRetrieved,
                              weak_method_factory_.GetWeakPtr(),
                              http_content_language, html_lang)];
  return true;
}

void LanguageDetectionController::OnTextRetrieved(
    const std::string& http_content_language,
    const std::string& html_lang,
    const base::string16& text_content) {
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = translate::DeterminePageLanguage(
      http_content_language, html_lang,
      GetStringByClippingLastWord(text_content,
                                  language_detection::kMaxIndexChars),
      &cld_language, &is_cld_reliable);
  if (language.empty())
    return;  // No language detected.

  DetectionDetails details;
  details.content_language = http_content_language;
  details.html_root_language = html_lang;
  details.adopted_language = language;
  details.cld_language = cld_language;
  details.is_cld_reliable = is_cld_reliable;
  language_detection_callbacks_.Notify(details);
}

void LanguageDetectionController::ExtractContentLanguageHeader(
    net::HttpResponseHeaders* headers) {
  if (!headers) {
    content_language_header_.clear();
    return;
  }

  headers->GetNormalizedHeader("content-language", &content_language_header_);
  // Remove everything after the comma ',' if any.
  size_t comma_index = content_language_header_.find_first_of(',');
  if (comma_index != std::string::npos)
    content_language_header_.resize(comma_index);
}

// web::WebStateObserver implementation:

void LanguageDetectionController::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS)
    StartLanguageDetection();
}

void LanguageDetectionController::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    StartLanguageDetection();
  } else {
    ExtractContentLanguageHeader(navigation_context->GetResponseHeaders());
  }
}

void LanguageDetectionController::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveScriptCommandCallback(kCommandPrefix);
}

}  // namespace translate
