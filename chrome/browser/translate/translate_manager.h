// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;
struct PageTranslatedDetails;
class PrefService;
struct TranslateErrorDetails;

namespace content {
class WebContents;
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

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Number of attempts before waiting for a page to be fully reloaded.
  void set_translate_max_reload_attemps(int attempts) {
    max_reload_check_attempts_ = attempts;
  }

  // Callback types for translate errors.
  typedef base::Callback<void(const TranslateErrorDetails&)>
      TranslateErrorCallback;
  typedef base::CallbackList<void(const TranslateErrorDetails&)>
      TranslateErrorCallbackList;

  // Registers a callback for translate errors.
  static scoped_ptr<TranslateErrorCallbackList::Subscription>
      RegisterTranslateErrorCallback(const TranslateErrorCallback& callback);

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

  void OnTranslateScriptFetchComplete(PendingRequest request,
                                      bool success,
                                      const std::string& data);

  content::NotificationRegistrar notification_registrar_;

  // Max number of attempts before checking if a page has been reloaded.
  int max_reload_check_attempts_;

  base::WeakPtrFactory<TranslateManager> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
