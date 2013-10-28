// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/language_state.h"

#include "chrome/browser/tab_contents/language_state_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationController;

LanguageState::LanguageState(NavigationController* nav_controller)
    : navigation_controller_(nav_controller),
      page_needs_translation_(false),
      translation_pending_(false),
      translation_declined_(false),
      in_page_navigation_(false),
      translate_enabled_(false),
      observer_(NULL) {
}

LanguageState::~LanguageState() {
}

void LanguageState::DidNavigate(
    const content::LoadCommittedDetails& details) {
  in_page_navigation_ = details.is_in_page;
  if (in_page_navigation_ || !details.is_main_frame)
    return;  // Don't reset our states, the page has not changed.

  bool reload =
      details.entry->GetTransitionType() == content::PAGE_TRANSITION_RELOAD ||
      details.type == content::NAVIGATION_TYPE_SAME_PAGE;
  if (reload) {
    // We might not get a LanguageDetermined notifications on reloads. Make sure
    // to keep the original language and to set current_lang_ so
    // IsPageTranslated() returns false.
    current_lang_ = original_lang_;
  } else {
    prev_original_lang_ = original_lang_;
    prev_current_lang_ = current_lang_;
    original_lang_.clear();
    current_lang_.clear();
  }

  translation_pending_ = false;
  translation_declined_ = false;

  SetTranslateEnabled(false);
}

void LanguageState::LanguageDetermined(const std::string& page_language,
                                       bool page_needs_translation) {
  if (in_page_navigation_ && !original_lang_.empty()) {
    // In-page navigation, we don't expect our states to change.
    // Note that we'll set the languages if original_lang_ is empty.  This might
    // happen if the we did not get called on the top-page.
    return;
  }
  page_needs_translation_ = page_needs_translation;
  original_lang_ = page_language;
  current_lang_ = page_language;
}

bool LanguageState::InTranslateNavigation() const {
  // The user is in the same translate session if
  //   - no translation is pending
  //   - this page is in the same language as the previous page
  //   - the previous page had been translated
  //   - the new page was navigated through a link.
  return
      !translation_pending_ &&
      prev_original_lang_ == original_lang_ &&
      prev_original_lang_ != prev_current_lang_ &&
      navigation_controller_->GetActiveEntry() &&
      navigation_controller_->GetActiveEntry()->GetTransitionType() ==
          content::PAGE_TRANSITION_LINK;
}


std::string LanguageState::AutoTranslateTo() const {
  if (InTranslateNavigation() &&
      // The page is not yet translated.
      original_lang_ == current_lang_ ) {
    return prev_current_lang_;
  }

  return std::string();
}

void LanguageState::SetTranslateEnabled(bool value) {
  if (translate_enabled_ == value)
    return;

  translate_enabled_ = value;
  if (observer_) {
    content::WebContents* web_contents =
        navigation_controller_->GetWebContents();
    observer_->OnTranslateEnabledChanged(web_contents);
  }
}

bool LanguageState::HasLanguageChanged() const {
  return original_lang_ != prev_original_lang_;
}
