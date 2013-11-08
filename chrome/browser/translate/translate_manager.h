// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/common/translate/translate_errors.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;
struct LanguageDetectionDetails;
struct PageTranslatedDetails;
class PrefService;
class Profile;
struct ShortcutConfiguration;
class TranslateAcceptLanguages;
struct TranslateErrorDetails;
struct TranslateEventDetails;
class TranslateInfoBarDelegate;
class TranslateLanguageList;
class TranslateScript;

namespace content {
class WebContents;
}

namespace net {
class URLFetcher;
}

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.
// It is a singleton.

class TranslateManager : public content::NotificationObserver {
 public:
  // Returns the singleton instance.
  static TranslateManager* GetInstance();

  virtual ~TranslateManager();

  // Returns true if the URL can be translated.
  static bool IsTranslatableURL(const GURL& url);

  // Fills |languages| with the list of languages that the translate server can
  // translate to and from.
  static void GetSupportedLanguages(std::vector<std::string>* languages);

  // Returns the last-updated time when Chrome receives a language list from a
  // Translate server. Returns null time if Chrome hasn't received any lists.
  static base::Time GetSupportedLanguagesLastUpdated();

  // Returns the language code that can be used with the Translate method for a
  // specified |chrome_locale|.
  static std::string GetLanguageCode(const std::string& chrome_locale);

  // Returns true if |language| is supported by the translation server.
  static bool IsSupportedLanguage(const std::string& language);

  // Returns true if |language| is supported by the translation server as a
  // alpha language.
  static bool IsAlphaLanguage(const std::string& language);

  // Returns true if |language| is an Accept language for the user profile.
  static bool IsAcceptLanguage(Profile* profile, const std::string& language);

  // Returns the language to translate to. The language returned is the
  // first language found in the following list that is supported by the
  // translation service:
  //     the UI language
  //     the accept-language list
  // If no language is found then an empty string is returned.
  static std::string GetTargetLanguage(PrefService* prefs);

  // Returns the language to automatically translate to. |original_language| is
  // the webpage's original language.
  static std::string GetAutoTargetLanguage(const std::string& original_language,
                                           PrefService* prefs);

  // Let the caller decide if and when we should fetch the language list from
  // the translate server. This is a NOOP if switches::kDisableTranslate is set
  // or if prefs::kEnableTranslate is set to false.
  void FetchLanguageListFromTranslateServer(PrefService* prefs);

  // Allows caller to cleanup pending URLFetcher objects to make sure they
  // get released in the appropriate thread... Mainly for tests.
  void CleanupPendingUlrFetcher();

  // Translates the page contents from |source_lang| to |target_lang|.
  // The actual translation might be performed asynchronously if the translate
  // script is not yet available.
  void TranslatePage(content::WebContents* web_contents,
                     const std::string& source_lang,
                     const std::string& target_lang);

  // Reverts the contents of the page in |web_contents| to its original
  // language.
  void RevertTranslation(content::WebContents* web_contents);

  // Reports to the Google translate server that a page language was incorrectly
  // detected.  This call is initiated by the user selecting the "report" menu
  // under options in the translate infobar.
  void ReportLanguageDetectionError(content::WebContents* web_contents);

  // Clears the translate script, so it will be fetched next time we translate.
  void ClearTranslateScript();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Used by unit-tests to override some defaults:
  // Delay after which the translate script is fetched again from the
  // translation server.
  void SetTranslateScriptExpirationDelay(int delay_ms);

  // Number of attempts before waiting for a page to be fully reloaded.
  void set_translate_max_reload_attemps(int attempts) {
    max_reload_check_attempts_ = attempts;
  }

  // The observer class for TranslateManager.
  class Observer {
   public:
    virtual void OnLanguageDetection(
        const LanguageDetectionDetails& details) = 0;
    virtual void OnTranslateError(
        const TranslateErrorDetails& details) = 0;
    virtual void OnTranslateEvent(
        const TranslateEventDetails& details) = 0;
  };

  // Adds/removes observer.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Notifies to the observers when translate event happens.
  void NotifyTranslateEvent(const TranslateEventDetails& details);

 protected:
  TranslateManager();

 private:
  friend struct DefaultSingletonTraits<TranslateManager>;

  // Structure that describes a translate request.
  // Translation may be deferred while the translate script is being retrieved
  // from the translate server.
  struct PendingRequest {
    int render_process_id;
    int render_view_id;
    int page_id;
    std::string source_lang;
    std::string target_lang;
  };

  // Starts the translation process on |tab| containing the page in the
  // |page_lang| language.
  void InitiateTranslation(content::WebContents* web_contents,
                           const std::string& page_lang);

  // If the tab identified by |process_id| and |render_id| has been closed, this
  // does nothing, otherwise it calls InitiateTranslation.
  void InitiateTranslationPosted(int process_id, int render_id,
                                 const std::string& page_lang, int attempt);

  // Sends a translation request to the RenderView of |web_contents|.
  void DoTranslatePage(content::WebContents* web_contents,
                       const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

  // Shows the after translate or error infobar depending on the details.
  void PageTranslated(content::WebContents* web_contents,
                      PageTranslatedDetails* details);

  void OnTranslateScriptFetchComplete(bool success, const std::string& data);

  // Notifies to the observers when a language is detected.
  void NotifyLanguageDetection(const LanguageDetectionDetails& details);

  // Notifies to the observers when translate failed.
  void NotifyTranslateError(const TranslateErrorDetails& details);

  // Shows the translate bubble.
  void ShowBubble(content::WebContents* web_contents,
                  TranslateBubbleModel::ViewState view_state);

  // Returns the different parameters used to decide whether extra shortcuts
  // are needed.
  static ShortcutConfiguration ShortcutConfig();

  content::NotificationRegistrar notification_registrar_;

  // Max number of attempts before checking if a page has been reloaded.
  int max_reload_check_attempts_;

  // The list of pending translate requests.  Translate requests are queued when
  // the translate script is not ready and has to be fetched from the translate
  // server.
  std::vector<PendingRequest> pending_requests_;

  // List of registered observers.
  ObserverList<Observer> observer_list_;

  // An instance of TranslateLanguageList which manages supported language list.
  scoped_ptr<TranslateLanguageList> language_list_;

  // An instance of TranslateScript which manages JavaScript source for
  // Translate.
  scoped_ptr<TranslateScript> script_;

  // An instance of TranslateAcceptLanguages which manages Accept languages of
  // each profiles.
  scoped_ptr<TranslateAcceptLanguages> accept_languages_;

  base::WeakPtrFactory<TranslateManager> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
