// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/translate_errors.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;
struct PageTranslatedDetails;
class PrefService;
class TranslateInfoBarDelegate;

namespace content {
class WebContents;
}

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.
// It is a singleton.

class TranslateManager : public content::NotificationObserver,
                         public content::URLFetcherDelegate {
 public:
  // Returns the singleton instance.
  static TranslateManager* GetInstance();

  virtual ~TranslateManager();

  // Let the caller decide if and when we should fetch the language list from
  // the translate server. This is a NOOP if switches::kDisableTranslate is
  // set or if prefs::kEnableTranslate is set to false.
  // It will not retry more than kMaxRetryLanguageListFetch times.
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
  void ClearTranslateScript() { translate_script_.clear(); }

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Used by unit-tests to override the default delay after which the translate
  // script is fetched again from the translation server.
  void set_translate_script_expiration_delay(int delay_ms) {
    translate_script_expiration_delay_ =
        base::TimeDelta::FromMilliseconds(delay_ms);
  }

  // Convenience method to know if a tab is showing a translate infobar.
  static bool IsShowingTranslateInfobar(content::WebContents* tab);

  // Returns true if the URL can be translated.
  static bool IsTranslatableURL(const GURL& url);

  // Fills |languages| with the list of languages that the translate server can
  // translate to and from.
  static void GetSupportedLanguages(std::vector<std::string>* languages);

  // Returns the language code that can be used with the Translate method for a
  // specified |chrome_locale|.
  static std::string GetLanguageCode(const std::string& chrome_locale);

  // Returns true if |language| is supported by the translation server.
  static bool IsSupportedLanguage(const std::string& language);

  // static const values shared with our browser tests.
  static const char* const kLanguageListCallbackName;
  static const char* const kTargetLanguagesKey;
 protected:
  TranslateManager();

 private:
  friend struct DefaultSingletonTraits<TranslateManager>;
  friend class TranslateManagerTest;
  FRIEND_TEST_ALL_PREFIXES(TranslateManagerTest, LanguageCodeSynonyms);

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

  // Fills supported_languages_ with the list of languages that the translate
  // server can translate to and from.
  static void SetSupportedLanguages(const std::string& language_list);

  // Initializes the list of supported languages if it wasn't initialized before
  // in case we failed to get them from the server, or didn't get them just yet.
  static void InitSupportedLanguages();

  // Starts the translation process on |tab| containing the page in the
  // |page_lang| language.
  void InitiateTranslation(content::WebContents* tab,
                           const std::string& page_lang);

  // If the tab identified by |process_id| and |render_id| has been closed, this
  // does nothing, otherwise it calls InitiateTranslation.
  void InitiateTranslationPosted(int process_id,
                                 int render_id,
                                 const std::string& page_lang);

  // Sends a translation request to the RenderView of |tab_contents|.
  void DoTranslatePage(content::WebContents* web_contents,
                       const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

  // Shows the after translate or error infobar depending on the details.
  void PageTranslated(content::WebContents* tab,
                      PageTranslatedDetails* details);

  // Returns true if the passed language has been configured by the user as an
  // accept language.
  bool IsAcceptLanguage(content::WebContents* tab, const std::string& language);

  // Initializes the |accept_languages_| language table based on the associated
  // preference in |prefs|.
  void InitAcceptLanguages(PrefService* prefs);

  // Fetches the JS translate script (the script that is injected in the page
  // to translate it).
  void RequestTranslateScript();

  // Shows the specified translate |infobar| in the given |tab|.  If a current
  // translate infobar is showing, it just replaces it with the new one.
  void ShowInfoBar(content::WebContents* tab,
                   TranslateInfoBarDelegate* infobar);

  // Returns the language to translate to. The language returned is the
  // first language found in the following list that is supported by the
  // translation service:
  //     the UI language
  //     the accept-language list
  // If no language is found then an empty string is returned.
  static std::string GetTargetLanguage(PrefService* prefs);

  // Returns the translate info bar showing in |tab| or NULL if none is showing.
  static TranslateInfoBarDelegate* GetTranslateInfoBarDelegate(
      content::WebContents* tab);

  content::NotificationRegistrar notification_registrar_;

  // Each PrefChangeRegistrar only tracks a single PrefService, so a map from
  // each PrefService used to its registrar is needed.
  typedef std::map<PrefService*, PrefChangeRegistrar*> PrefServiceRegistrarMap;
  PrefServiceRegistrarMap pref_change_registrars_;

  // A map that associates a profile with its parsed "accept languages".
  typedef std::set<std::string> LanguageSet;
  typedef std::map<PrefService*, LanguageSet> PrefServiceLanguagesMap;
  PrefServiceLanguagesMap accept_languages_;

  base::WeakPtrFactory<TranslateManager> weak_method_factory_;

  // The JS injected in the page to do the translation.
  std::string translate_script_;

  // Delay after which the translate script is fetched again
  // from the translate server.
  base::TimeDelta translate_script_expiration_delay_;

  // Set when the translate JS is currently being retrieved. NULL otherwise.
  scoped_ptr<content::URLFetcher> translate_script_request_pending_;

  // Set when the list of languages is currently being retrieved.
  // NULL otherwise.
  scoped_ptr<content::URLFetcher> language_list_request_pending_;

  // The list of pending translate requests.  Translate requests are queued when
  // the translate script is not ready and has to be fetched from the translate
  // server.
  std::vector<PendingRequest> pending_requests_;

  // The languages supported by the translation server.
  static base::LazyInstance<std::set<std::string> > supported_languages_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
