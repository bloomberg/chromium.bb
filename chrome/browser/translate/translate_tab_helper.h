// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
#pragma once

#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/common/translate_errors.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class TranslateTabHelper : public TabContentsObserver {
 public:
  explicit TranslateTabHelper(TabContents* tab_contents);
  virtual ~TranslateTabHelper();

  LanguageState& language_state() { return language_state_; }

 private:
  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  void OnLanguageDetermined(const std::string& language,
                            bool page_translatable);
  void OnPageTranslated(int32 page_id,
                        const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type);

  // Information about the language the page is in and has been translated to.
  LanguageState language_state_;

  DISALLOW_COPY_AND_ASSIGN(TranslateTabHelper);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_TAB_HELPER_H_
