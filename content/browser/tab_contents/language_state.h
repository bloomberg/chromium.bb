// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_LANGUAGE_STATE_H_
#define CONTENT_BROWSER_TAB_CONTENTS_LANGUAGE_STATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/browser/tab_contents/navigation_controller.h"

// This class holds the language state of the current page.
// There is one LanguageState instance per TabContents.
// It is used to determine when navigating to a new page whether it should
// automatically be translated.
// This auto-translate behavior is the expected behavior when:
// - user is on page in language A that they had translated to language B.
// - user clicks a link in that page that takes them to a page also in language
//   A.

class LanguageState {
 public:
  explicit LanguageState(NavigationController* nav_controller);
  ~LanguageState();

  // Should be called when the page did a new navigation (whether it is a main
  // frame or sub-frame navigation).
  void DidNavigate(const NavigationController::LoadCommittedDetails& details);

  // Should be called when the language of the page has been determined.
  // |page_translatable| when false indicates that the browser should not offer
  // to translate the page.
  void LanguageDetermined(const std::string& page_language,
                          bool page_translatable);

  // Returns the language the current page should be translated to, based on the
  // previous page languages and the transition.  This should be called after
  // the language page has been determined.
  // Returns an empty string if the page should not be auto-translated.
  std::string AutoTranslateTo() const;

  // Returns true if the current page in the associated tab has been translated.
  bool IsPageTranslated() const { return original_lang_ != current_lang_; }

  const std::string& original_language() const { return original_lang_; }

  void set_current_language(const std::string& language) {
    current_lang_ = language;
  }
  const std::string& current_language() const { return current_lang_; }

  bool page_translatable() const { return page_translatable_; }

  // Whether the page is currently in the process of being translated.
  bool translation_pending() const { return translation_pending_; }
  void set_translation_pending(bool value) { translation_pending_ = value; }

  // Whether the user has already declined to translate the page.
  bool translation_declined() const { return translation_declined_; }
  void set_translation_declined(bool value) { translation_declined_ = value; }

  // Whether the current page was navigated through an in-page (fragment)
  // navigation.
  bool in_page_navigation() const { return in_page_navigation_; }

 private:
  // The languages this page is in. Note that current_lang_ is different from
  // original_lang_ when the page has been translated.
  // Note that these might be empty if the page language has not been determined
  // yet.
  std::string original_lang_;
  std::string current_lang_;

  // Same as above but for the previous page.
  std::string prev_original_lang_;
  std::string prev_current_lang_;

  // The navigation controller of the tab we are associated with.
  NavigationController* navigation_controller_;

  // Whether it is OK to offer to translate the page.  Some pages explictly
  // specify that they should not be translated by the browser (this is the case
  // for GMail for example, which provides its own translation features).
  bool page_translatable_;

  // Whether a translation is currently pending (TabContents waiting for the
  // PAGE_TRANSLATED notification).  This is needed to avoid sending duplicate
  // translate requests to a page.  TranslateManager initiates translations
  // when it received the LANGUAGE_DETERMINED notification.  This is sent by
  // the renderer with the page contents, every time the load stops for the
  // main frame, so we may get several.
  // TODO(jcampan): make the renderer send the language just once per navigation
  //                then we can get rid of that state.
  bool translation_pending_;

  // Whether the user has declined to translate the page (by closing the infobar
  // for example).  This is necessary as a new infobar could be shown if a new
  // load happens in the page after the user closed the infobar.
  bool translation_declined_;

  // Whether the current navigation is a fragment navigation (in page).
  bool in_page_navigation_;

  DISALLOW_COPY_AND_ASSIGN(LanguageState);
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_LANGUAGE_STATE_H_
