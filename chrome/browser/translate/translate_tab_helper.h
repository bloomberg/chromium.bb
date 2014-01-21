// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_

#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

struct LanguageDetectionDetails;

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

 private:
  explicit TranslateTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TranslateTabHelper>;

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  void OnLanguageDetermined(const LanguageDetectionDetails& details,
                            bool page_needs_translation);
  void OnPageTranslated(int32 page_id,
                        const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type);

  ContentTranslateDriver translate_driver_;

  DISALLOW_COPY_AND_ASSIGN(TranslateTabHelper);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
