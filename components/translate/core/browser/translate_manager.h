// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_errors.h"

class GURL;
class PrefService;

namespace translate {

class TranslateClient;
class TranslateDriver;
class TranslatePrefs;
struct TranslateErrorDetails;

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.

class TranslateManager {
 public:
  // |translate_client| is expected to outlive the TranslateManager.
  // |accept_language_pref_name| is the path for the preference for the
  // accept-languages.
  TranslateManager(TranslateClient* translate_client,
                   const std::string& accept_language_pref_name);
  virtual ~TranslateManager();

  // Returns a weak pointer to this instance.
  base::WeakPtr<TranslateManager> GetWeakPtr();

  // Cannot return NULL.
  TranslateClient* translate_client() { return translate_client_; }

  // Sets the sequence number of the current page, for use while sending
  // messages to the renderer.
  void set_current_seq_no(int page_seq_no) { page_seq_no_ = page_seq_no; }

  // Returns the language to translate to. The language returned is the
  // first language found in the following list that is supported by the
  // translation service:
  //     the UI language
  //     the accept-language list
  // If no language is found then an empty string is returned.
  static std::string GetTargetLanguage(
      const std::vector<std::string>& accept_languages_list);

  // Returns the language to automatically translate to. |original_language| is
  // the webpage's original language.
  static std::string GetAutoTargetLanguage(const std::string& original_language,
                                           TranslatePrefs* translate_prefs);

  // Translates the page contents from |source_lang| to |target_lang|.
  // The actual translation might be performed asynchronously if the translate
  // script is not yet available.
  void TranslatePage(const std::string& source_lang,
                     const std::string& target_lang,
                     bool triggered_from_menu);

  // Starts the translation process for the page in the |page_lang| language.
  void InitiateTranslation(const std::string& page_lang);

  // Shows the after translate or error infobar depending on the details.
  void PageTranslated(const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateErrors::Type error_type);

  // Reverts the contents of the page to its original language.
  void RevertTranslation();

  // Reports to the Google translate server that a page language was incorrectly
  // detected.  This call is initiated by the user selecting the "report" menu
  // under options in the translate infobar.
  void ReportLanguageDetectionError();

  // Callback types for translate errors.
  typedef base::Callback<void(const TranslateErrorDetails&)>
      TranslateErrorCallback;
  typedef base::CallbackList<void(const TranslateErrorDetails&)>
      TranslateErrorCallbackList;

  // Registers a callback for translate errors.
  static scoped_ptr<TranslateErrorCallbackList::Subscription>
      RegisterTranslateErrorCallback(const TranslateErrorCallback& callback);

  // Gets the LanguageState associated with the TranslateManager
  LanguageState& GetLanguageState();

 private:
  // Sends a translation request to the TranslateDriver.
  void DoTranslatePage(const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

  // Called when the Translate script has been fetched.
  // Initiates the translation.
  void OnTranslateScriptFetchComplete(const std::string& source_lang,
                                      const std::string& target_lang,
                                      bool success,
                                      const std::string& data);

  // Sequence number of the current page.
  int page_seq_no_;

  // Preference name for the Accept-Languages HTTP header.
  std::string accept_languages_pref_name_;

  TranslateClient* translate_client_;  // Weak.
  TranslateDriver* translate_driver_;  // Weak.

  LanguageState language_state_;

  base::WeakPtrFactory<TranslateManager> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_
