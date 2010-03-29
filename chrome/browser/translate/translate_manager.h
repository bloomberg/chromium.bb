// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/singleton.h"
#include "base/task.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class PrefService;
class TabContents;

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.
// It is a singleton.

class TranslateManager : public NotificationObserver {
 public:
  virtual ~TranslateManager();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Convenience method to know if a tab is showing a translate infobar.
  static bool IsShowingTranslateInfobar(TabContents* tab);

  // Returns true if the URL can be translated, if it is not an internal URL
  // (chrome:// and others).
  static bool IsTranslatableURL(const GURL& url);

  // Used by unit-test to enable the TranslateManager for testing purpose.
  static void set_test_enabled(bool enabled) { test_enabled_ = enabled; }
  static bool test_enabled() { return test_enabled_; }

 protected:
  TranslateManager();

 private:
  friend struct DefaultSingletonTraits<TranslateManager>;

  // Starts the translation process on |tab| containing the page in the
  // |page_lang| language.
  void InitiateTranslation(TabContents* tab, const std::string& page_lang);

  // If the tab identified by |process_id| and |render_id| has been closed, this
  // does nothing, otherwise it calls InitiateTranslation.
  void InitiateTranslationPosted(int process_id,
                                 int render_id,
                                 const std::string& page_lang);

  // Returns true if the passed language has been configured by the user as an
  // accept language.
  bool IsAcceptLanguage(TabContents* tab, const std::string& language);

  // Initializes the |accept_languages_| language table based on the associated
  // preference in |prefs|.
  void InitAcceptLanguages(PrefService* prefs);

  // Convenience method that adds a translate infobar to |tab|.
  static void AddTranslateInfoBar(
      TabContents* tab,
      TranslateInfoBarDelegate::TranslateState state,
      const GURL& url,
      const std::string& original_language,
      const std::string& target_language);

  // Returns the language to translate to, which is the language the UI is
  // configured in.  Returns an empty string if that language is not supported
  // by the translation service.
  static std::string GetTargetLanguage();

  NotificationRegistrar notification_registrar_;

  // A map that associates a profile with its parsed "accept languages".
  typedef std::set<std::string> LanguageSet;
  typedef std::map<PrefService*, LanguageSet> PrefServiceLanguagesMap;
  PrefServiceLanguagesMap accept_languages_;

  ScopedRunnableMethodFactory<TranslateManager> method_factory_;

  static bool test_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
