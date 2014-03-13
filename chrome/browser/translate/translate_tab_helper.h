// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class BrowserContext;
class WebContents;
}

struct LanguageDetectionDetails;
class PrefService;
class TranslateAcceptLanguages;
class TranslatePrefs;
class TranslateManager;

class TranslateTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TranslateTabHelper> {
 public:
  virtual ~TranslateTabHelper();

  // Gets the LanguageState associated with the page.
  LanguageState& GetLanguageState();

  // Returns the ContentTranslateDriver instance associated with this
  // WebContents.
  ContentTranslateDriver& translate_driver() { return translate_driver_; }

  // Helper method to return a new TranslatePrefs instance.
  static scoped_ptr<TranslatePrefs> CreateTranslatePrefs(PrefService* prefs);

  // Helper method to return the TranslateAcceptLanguages instance associated
  // with |browser_context|.
  static TranslateAcceptLanguages* GetTranslateAcceptLanguages(
      content::BrowserContext* browser_context);

  // Helper method to return the TranslateManager instance associated with
  // |web_contents|, or NULL if there is no such associated instance.
  static TranslateManager* GetManagerFromWebContents(
      content::WebContents* web_contents);

  // Gets the associated TranslateManager.
  TranslateManager* GetTranslateManager();

  // Gets the associated WebContents. Returns NULL if the WebContents is being
  // destroyed.
  content::WebContents* GetWebContents();

  // Denotes which state the user is in with respect to translate.
  enum TranslateStep {
    BEFORE_TRANSLATE,
    TRANSLATING,
    AFTER_TRANSLATE,
    TRANSLATE_ERROR
  };

  // Called when the embedder should present UI to the user corresponding to the
  // user's current |step|.
  void ShowTranslateUI(TranslateStep step,
                       const std::string source_language,
                       const std::string target_language,
                       TranslateErrors::Type error_type,
                       bool triggered_from_menu);

 private:
  explicit TranslateTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TranslateTabHelper>;

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  void OnLanguageDetermined(const LanguageDetectionDetails& details,
                            bool page_needs_translation);
  void OnPageTranslated(int32 page_id,
                        const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type);

  // Shows the translate bubble.
  void ShowBubble(TranslateStep step, TranslateErrors::Type error_type);

  ContentTranslateDriver translate_driver_;
  scoped_ptr<TranslateManager> translate_manager_;

  DISALLOW_COPY_AND_ASSIGN(TranslateTabHelper);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
