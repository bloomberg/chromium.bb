// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/task.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/translate_errors.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;
struct PageTranslatedDetails;
class PrefService;
class TabContents;
class TranslateInfoBarDelegate;

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.
// It is a singleton.

class TranslateManager : public NotificationObserver,
                         public URLFetcher::Delegate {
 public:
  // Returns the singleton instance.
  static TranslateManager* GetInstance();

  virtual ~TranslateManager();

  // Translates the page contents from |source_lang| to |target_lang|.
  // The actual translation might be performed asynchronously if the translate
  // script is not yet available.
  void TranslatePage(TabContents* tab_contents,
                     const std::string& source_lang,
                     const std::string& target_lang);

  // Reverts the contents of the page in |tab_contents| to its original
  // language.
  void RevertTranslation(TabContents* tab_contents);

  // Reports to the Google translate server that a page language was incorrectly
  // detected.  This call is initiated by the user selecting the "report" menu
  // under options in the translate infobar.
  void ReportLanguageDetectionError(TabContents* tab_contents);

  // Clears the translate script, so it will be fetched next time we translate.
  void ClearTranslateScript() { translate_script_.clear(); }

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // URLFetcher::Delegate implementation:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Used by unit-tests to override the default delay after which the translate
  // script is fetched again from the translation server.
  void set_translate_script_expiration_delay(int delay_ms) {
    translate_script_expiration_delay_ = delay_ms;
  }

  // Convenience method to know if a tab is showing a translate infobar.
  static bool IsShowingTranslateInfobar(TabContents* tab);

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
  void InitiateTranslation(TabContents* tab, const std::string& page_lang);

  // If the tab identified by |process_id| and |render_id| has been closed, this
  // does nothing, otherwise it calls InitiateTranslation.
  void InitiateTranslationPosted(int process_id,
                                 int render_id,
                                 const std::string& page_lang);

  // Sends a translation request to the RenderView of |tab_contents|.
  void DoTranslatePage(TabContents* tab_contents,
                       const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

   // Shows the after translate or error infobar depending on the details.
   void PageTranslated(TabContents* tab, PageTranslatedDetails* details);

  // Returns true if the passed language has been configured by the user as an
  // accept language.
  bool IsAcceptLanguage(TabContents* tab, const std::string& language);

  // Initializes the |accept_languages_| language table based on the associated
  // preference in |prefs|.
  void InitAcceptLanguages(PrefService* prefs);

  // Fetches the JS translate script (the script that is injected in the page
  // to translate it).
  void RequestTranslateScript();

  // Shows the specified translate |infobar| in the given |tab|.  If a current
  // translate infobar is showing, it just replaces it with the new one.
  void ShowInfoBar(TabContents* tab, TranslateInfoBarDelegate* infobar);

  // Returns the language to translate to, which is the language the UI is
  // configured in.  Returns an empty string if that language is not supported
  // by the translation service.
  static std::string GetTargetLanguage();

  // Returns the translate info bar showing in |tab| or NULL if none is showing.
  static TranslateInfoBarDelegate* GetTranslateInfoBarDelegate(
      TabContents* tab);

  NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // A map that associates a profile with its parsed "accept languages".
  typedef std::set<std::string> LanguageSet;
  typedef std::map<PrefService*, LanguageSet> PrefServiceLanguagesMap;
  PrefServiceLanguagesMap accept_languages_;

  ScopedRunnableMethodFactory<TranslateManager> method_factory_;

  // The JS injected in the page to do the translation.
  std::string translate_script_;

  // Delay in milli-seconds after which the translate script is fetched again
  // from the translate server.
  int translate_script_expiration_delay_;

  // Whether the translate JS is currently being retrieved.
  bool translate_script_request_pending_;

  // The list of pending translate requests.  Translate requests are queued when
  // the translate script is not ready and has to be fetched from the translate
  // server.
  std::vector<PendingRequest> pending_requests_;

  // The languages supported by the translation server.
  static base::LazyInstance<std::set<std::string> > supported_languages_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
