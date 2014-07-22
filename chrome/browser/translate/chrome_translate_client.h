// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_
#define CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class File;
}  // namespace base

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace test {
class ScopedCLDDynamicDataHarness;
}  // namespace test

class PrefService;

namespace translate {
struct LanguageDetectionDetails;
class LanguageState;
class TranslateAcceptLanguages;
class TranslatePrefs;
class TranslateManager;
}  // namespace translate

class ChromeTranslateClient
    : public translate::TranslateClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeTranslateClient> {
 public:
  virtual ~ChromeTranslateClient();

  // Gets the LanguageState associated with the page.
  translate::LanguageState& GetLanguageState();

  // Returns the ContentTranslateDriver instance associated with this
  // WebContents.
  translate::ContentTranslateDriver& translate_driver() {
    return translate_driver_;
  }

  // Helper method to return a new TranslatePrefs instance.
  static scoped_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
      PrefService* prefs);

  // Helper method to return the TranslateAcceptLanguages instance associated
  // with |browser_context|.
  static translate::TranslateAcceptLanguages* GetTranslateAcceptLanguages(
      content::BrowserContext* browser_context);

  // Helper method to return the TranslateManager instance associated with
  // |web_contents|, or NULL if there is no such associated instance.
  static translate::TranslateManager* GetManagerFromWebContents(
      content::WebContents* web_contents);

  // Gets |source| and |target| language for translation.
  static void GetTranslateLanguages(content::WebContents* web_contents,
                                    std::string* source,
                                    std::string* target);

  // Gets the associated TranslateManager.
  translate::TranslateManager* GetTranslateManager();

  // Gets the associated WebContents. Returns NULL if the WebContents is being
  // destroyed.
  content::WebContents* GetWebContents();

  // Number of attempts before waiting for a page to be fully reloaded.
  void set_translate_max_reload_attempts(int attempts) {
    max_reload_check_attempts_ = attempts;
  }

  // TranslateClient implementation.
  virtual translate::TranslateDriver* GetTranslateDriver() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual scoped_ptr<translate::TranslatePrefs> GetTranslatePrefs() OVERRIDE;
  virtual translate::TranslateAcceptLanguages* GetTranslateAcceptLanguages()
      OVERRIDE;
  virtual int GetInfobarIconID() const OVERRIDE;
  virtual scoped_ptr<infobars::InfoBar> CreateInfoBar(
      scoped_ptr<translate::TranslateInfoBarDelegate> delegate) const OVERRIDE;
  virtual void ShowTranslateUI(translate::TranslateStep step,
                               const std::string source_language,
                               const std::string target_language,
                               translate::TranslateErrors::Type error_type,
                               bool triggered_from_menu) OVERRIDE;
  virtual bool IsTranslatableURL(const GURL& url) OVERRIDE;
  virtual void ShowReportLanguageDetectionErrorUI(
      const GURL& report_url) OVERRIDE;

 private:
  explicit ChromeTranslateClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromeTranslateClient>;

  // content::WebContentsObserver implementation.
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // Initiates translation once the page is finished loading.
  void InitiateTranslation(const std::string& page_lang, int attempt);

  // IPC handlers.
  void OnTranslateAssignedSequenceNumber(int page_seq_no);
  void OnLanguageDetermined(const translate::LanguageDetectionDetails& details,
                            bool page_needs_translation);
  void OnPageTranslated(const std::string& original_lang,
                        const std::string& translated_lang,
                        translate::TranslateErrors::Type error_type);

  // Shows the translate bubble.
  void ShowBubble(translate::TranslateStep step,
                  translate::TranslateErrors::Type error_type);

  // Max number of attempts before checking if a page has been reloaded.
  int max_reload_check_attempts_;

  translate::ContentTranslateDriver translate_driver_;
  scoped_ptr<translate::TranslateManager> translate_manager_;

  // Provides CLD data for this process.
  scoped_ptr<translate::BrowserCldDataProvider> cld_data_provider_;

  base::WeakPtrFactory<ChromeTranslateClient> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTranslateClient);
};

#endif  // CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_
