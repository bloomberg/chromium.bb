// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_tab_helper.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/render_messages.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

namespace {

// Converts from TranslateTabHelper::TranslateStep to
// TranslateBubbleModel::ViewState.
TranslateBubbleModel::ViewState BubbleViewStateFromStep(
    TranslateTabHelper::TranslateStep step,
    TranslateErrors::Type error_type) {
  if (error_type != TranslateErrors::NONE)
    return TranslateBubbleModel::VIEW_STATE_ERROR;

  switch (step) {
    case TranslateTabHelper::BEFORE_TRANSLATE:
      return TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE;
    case TranslateTabHelper::TRANSLATING:
      return TranslateBubbleModel::VIEW_STATE_TRANSLATING;
    case TranslateTabHelper::AFTER_TRANSLATE:
      return TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE;
    case TranslateTabHelper::TRANSLATE_ERROR:
      NOTREACHED() << "error_type should not be NONE";
      return TranslateBubbleModel::VIEW_STATE_ERROR;
  }

  // This return statement is here to avoid some compilers to incorrectly
  // complain about reaching the end of a non-void function.
  NOTREACHED();
  return TranslateBubbleModel::VIEW_STATE_ERROR;
}

// Converts from TranslateTabHelper::TranslateStep to
// TranslateInfoBarDelegate::Type.
TranslateInfoBarDelegate::Type InfobarTypeFromStep(
    TranslateTabHelper::TranslateStep step,
    TranslateErrors::Type error_type) {
  if (error_type != TranslateErrors::NONE)
    return TranslateInfoBarDelegate::TRANSLATION_ERROR;

  switch (step) {
    case TranslateTabHelper::BEFORE_TRANSLATE:
      return TranslateInfoBarDelegate::BEFORE_TRANSLATE;
    case TranslateTabHelper::TRANSLATING:
      return TranslateInfoBarDelegate::TRANSLATING;
    case TranslateTabHelper::AFTER_TRANSLATE:
      return TranslateInfoBarDelegate::AFTER_TRANSLATE;
    case TranslateTabHelper::TRANSLATE_ERROR:
      NOTREACHED() << "error_type should not be NONE";
      return TranslateInfoBarDelegate::TRANSLATION_ERROR;
  }

  // This return statement is here to avoid some compilers to incorrectly
  // complain about reaching the end of a non-void function.
  NOTREACHED();
  return TranslateInfoBarDelegate::TRANSLATION_ERROR;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TranslateTabHelper);

TranslateTabHelper::TranslateTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      translate_driver_(&web_contents->GetController()) {
}

TranslateTabHelper::~TranslateTabHelper() {
}

LanguageState& TranslateTabHelper::GetLanguageState() {
  return translate_driver_.language_state();
}

void TranslateTabHelper::ShowTranslateUI(TranslateTabHelper::TranslateStep step,
                                         content::WebContents* web_contents,
                                         const std::string source_language,
                                         const std::string target_language,
                                         TranslateErrors::Type error_type) {
  if (TranslateService::IsTranslateBubbleEnabled()) {
    // Bubble UI.
    TranslateBubbleModel::ViewState view_state =
        BubbleViewStateFromStep(step, error_type);
    if (view_state == TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
      // TODO: Move this logic out of UI code.
      GetLanguageState().SetTranslateEnabled(true);
      if (!GetLanguageState().HasLanguageChanged())
        return;
    }
    ShowBubble(web_contents, view_state, error_type);
    return;
  }

  // Infobar UI.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  PrefService* prefs = original_profile->GetPrefs();

  TranslateInfoBarDelegate::Type infobar_type =
      InfobarTypeFromStep(step, error_type);
  bool replace_existing =
      infobar_type != TranslateInfoBarDelegate::BEFORE_TRANSLATE;

  TranslateInfoBarDelegate::Create(replace_existing,
                                   web_contents,
                                   infobar_type,
                                   source_language,
                                   target_language,
                                   error_type,
                                   prefs);
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
  translate_driver_.DidNavigate(details);
}

void TranslateTabHelper::OnLanguageDetermined(
    const LanguageDetectionDetails& details,
    bool page_needs_translation) {
  translate_driver_.language_state().LanguageDetermined(
      details.adopted_language, page_needs_translation);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<const LanguageDetectionDetails>(&details));
}

void TranslateTabHelper::OnPageTranslated(int32 page_id,
                                          const std::string& original_lang,
                                          const std::string& translated_lang,
                                          TranslateErrors::Type error_type) {
  translate_driver_.language_state().SetCurrentLanguage(translated_lang);
  translate_driver_.language_state().set_translation_pending(false);
  PageTranslatedDetails details;
  details.source_language = original_lang;
  details.target_language = translated_lang;
  details.error_type = error_type;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<PageTranslatedDetails>(&details));
}

void TranslateTabHelper::ShowBubble(content::WebContents* web_contents,
                                    TranslateBubbleModel::ViewState view_state,
                                    TranslateErrors::Type error_type) {
// The bubble is implemented only on the desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    TranslateBubbleFactory::Show(NULL, web_contents, view_state, error_type);
    return;
  }

  if (web_contents != browser->tab_strip_model()->GetActiveWebContents())
    return;

  // This ShowBubble function is also used for upating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser !=
      chrome::FindLastActiveWithHostDesktopType(browser->host_desktop_type())) {
    return;
  }

  // During auto-translating, the bubble should not be shown.
  if (view_state == TranslateBubbleModel::VIEW_STATE_TRANSLATING ||
      view_state == TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE) {
    if (GetLanguageState().InTranslateNavigation())
      return;
  }

  TranslateBubbleFactory::Show(
      browser->window(), web_contents, view_state, error_type);
#else
  NOTREACHED();
#endif
}
