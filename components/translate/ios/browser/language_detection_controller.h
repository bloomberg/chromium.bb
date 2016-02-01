// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_

#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
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

namespace web {
class WebState;
}

namespace translate {

class LanguageDetectionController : public web::WebStateObserver {
 public:
  // Language detection details, passed to language detection callbacks.
  struct DetectionDetails {
    // The language detected by the content (Content-Language).
    std::string content_language;

    // The language written in the lang attribute of the html element.
    std::string html_root_language;

    // The adopted language.
    std::string adopted_language;
  };

  LanguageDetectionController(web::WebState* web_state,
                              JsLanguageDetectionManager* manager,
                              PrefService* prefs);
  ~LanguageDetectionController() override;

  // Callback types for language detection events.
  typedef base::Callback<void(const DetectionDetails&)> Callback;
  typedef base::CallbackList<void(const DetectionDetails&)> CallbackList;

  // Registers a callback for language detection events.
  scoped_ptr<CallbackList::Subscription> RegisterLanguageDetectionCallback(
      const Callback& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(LanguageDetectionControllerTest, OnTextCaptured);

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

  // web::WebStateObserver implementation:
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;
  void UrlHashChanged() override;
  void HistoryStateChanged() override;
  void WebStateDestroyed() override;

  CallbackList language_detection_callbacks_;
  base::scoped_nsobject<JsLanguageDetectionManager> js_manager_;
  BooleanPrefMember translate_enabled_;
  base::WeakPtrFactory<LanguageDetectionController> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(LanguageDetectionController);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_CONTROLLER_H_
