// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_tab_helper.h"

#include "chrome/browser/translate/page_translated_details.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TranslateTabHelper);

TranslateTabHelper::TranslateTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      language_state_(&web_contents->GetController()) {
}

TranslateTabHelper::~TranslateTabHelper() {
}

bool TranslateTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TranslateTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_TranslateLanguageDetermined,
                        OnLanguageDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PageTranslated, OnPageTranslated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void TranslateTabHelper::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Let the LanguageState clear its state.
  language_state_.DidNavigate(details);
}

void TranslateTabHelper::OnLanguageDetermined(const std::string& language,
                                              bool page_translatable) {
  language_state_.LanguageDetermined(language, page_translatable);

  std::string lang = language;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::Source<WebContents>(web_contents()),
      content::Details<std::string>(&lang));
}

void TranslateTabHelper::OnPageTranslated(int32 page_id,
                                          const std::string& original_lang,
                                          const std::string& translated_lang,
                                          TranslateErrors::Type error_type) {
  language_state_.set_current_language(translated_lang);
  language_state_.set_translation_pending(false);
  PageTranslatedDetails details(original_lang, translated_lang, error_type);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::Source<WebContents>(web_contents()),
      content::Details<PageTranslatedDetails>(&details));
}
