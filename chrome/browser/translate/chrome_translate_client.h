// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_
#define CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace test {
class ScopedCLDDynamicDataHarness;
}  // namespace test

class PrefService;

namespace translate {
class LanguageState;
class TranslateAcceptLanguages;
class TranslatePrefs;
class TranslateManager;
}  // namespace translate

class ChromeTranslateClient
    : public translate::TranslateClient,
      public translate::ContentTranslateDriver::Observer,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeTranslateClient> {
 public:
  ~ChromeTranslateClient() override;

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

  // TranslateClient implementation.
  translate::TranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  scoped_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  translate::TranslateAcceptLanguages* GetTranslateAcceptLanguages() override;
  int GetInfobarIconID() const override;
#if !defined(USE_AURA)
  scoped_ptr<infobars::InfoBar> CreateInfoBar(
      scoped_ptr<translate::TranslateInfoBarDelegate> delegate) const override;
#endif
  void ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors::Type error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;
  void ShowReportLanguageDetectionErrorUI(const GURL& report_url) override;

  // ContentTranslateDriver::Observer implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnPageTranslated(const std::string& original_lang,
                        const std::string& translated_lang,
                        translate::TranslateErrors::Type error_type) override;

 private:
  explicit ChromeTranslateClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromeTranslateClient>;

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void WebContentsDestroyed() override;

  // Shows the translate bubble.
  void ShowBubble(translate::TranslateStep step,
                  translate::TranslateErrors::Type error_type);

  translate::ContentTranslateDriver translate_driver_;
  scoped_ptr<translate::TranslateManager> translate_manager_;

  // Provides CLD data for this process.
  scoped_ptr<translate::BrowserCldDataProvider> cld_data_provider_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTranslateClient);
};

#endif  // CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_
