// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_member.h"
#include "ios/web/public/web_state/web_state_observer.h"

class GURL;
@class JsLanguageDetectionManager;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace net {
class HttpResponseHeaders;
}

namespace web {
class NavigationContext;
class WebState;
}

namespace translate {

class LanguageDetectionController : public web::WebStateObserver {
 public:
  // Language detection details, passed to language detection callbacks.
  // TODO(crbug.com/715447): Investigate if we can use the existing
  // detection_details under
  // components/translate/core/common/language_detection_details.h.
  struct DetectionDetails {
    DetectionDetails();
    DetectionDetails(const DetectionDetails& other);
    ~DetectionDetails();

    // The language detected by the content (Content-Language).
    std::string content_language;

    // The language written in the lang attribute of the html element.
    std::string html_root_language;

    // The adopted language.
    std::string adopted_language;

    // The language detected by CLD.
    std::string cld_language;

    // Whether the CLD detection is reliable or not.
    bool is_cld_reliable;
  };

  LanguageDetectionController(web::WebState* web_state,
                              JsLanguageDetectionManager* manager,
                              PrefService* prefs);
  ~LanguageDetectionController() override;

  // Callback types for language detection events.
  typedef base::Callback<void(const DetectionDetails&)> Callback;
  typedef base::CallbackList<void(const DetectionDetails&)> CallbackList;

  // Registers a callback for language detection events.
  std::unique_ptr<CallbackList::Subscription> RegisterLanguageDetectionCallback(
      const Callback& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(LanguageDetectionControllerTest, OnTextCaptured);
  FRIEND_TEST_ALL_PREFIXES(LanguageDetectionControllerTest,
                           MissingHttpContentLanguage);

  // Starts the page language detection and initiates the translation process.
  void StartLanguageDetection();

  // Handles the "languageDetection.textCaptured" javascript command.
  // |interacting| is true if the user is currently interacting with the page.
  bool OnTextCaptured(const base::DictionaryValue& value,
                      const GURL& url,
                      bool interacting);

  // Completion handler used to retrieve the text buffered by the
  // JsLanguageDetectionManager.
  void OnTextRetrieved(const std::string& http_content_language,
                       const std::string& html_lang,
                       const base::string16& text);

  // Extracts "content-language" header into content_language_header_ variable.
  void ExtractContentLanguageHeader(net::HttpResponseHeaders* headers);

  // web::WebStateObserver implementation:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  CallbackList language_detection_callbacks_;
  JsLanguageDetectionManager* js_manager_;
  BooleanPrefMember translate_enabled_;
  std::string content_language_header_;
  base::WeakPtrFactory<LanguageDetectionController> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(LanguageDetectionController);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
