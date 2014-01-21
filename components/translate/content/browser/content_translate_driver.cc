// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include "base/logging.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"

ContentTranslateDriver::ContentTranslateDriver(
    content::NavigationController* nav_controller)
    : navigation_controller_(nav_controller),
      language_state_(this),
      observer_(NULL) {
  DCHECK(navigation_controller_);
}

ContentTranslateDriver::~ContentTranslateDriver() {}

void ContentTranslateDriver::DidNavigate(
    const content::LoadCommittedDetails& details) {
  const bool reload =
      details.entry->GetTransitionType() == content::PAGE_TRANSITION_RELOAD ||
      details.type == content::NAVIGATION_TYPE_SAME_PAGE;
  language_state_.DidNavigate(
      details.is_in_page, details.is_main_frame, reload);
}

// TranslateDriver methods

bool ContentTranslateDriver::IsLinkNavigation() {
  return navigation_controller_ && navigation_controller_->GetActiveEntry() &&
         navigation_controller_->GetActiveEntry()->GetTransitionType() ==
             content::PAGE_TRANSITION_LINK;
}

void ContentTranslateDriver::OnTranslateEnabledChanged() {
  if (observer_) {
    content::WebContents* web_contents =
        navigation_controller_->GetWebContents();
    observer_->OnTranslateEnabledChanged(web_contents);
  }
}

void ContentTranslateDriver::OnIsPageTranslatedChanged() {
  if (observer_) {
    content::WebContents* web_contents =
        navigation_controller_->GetWebContents();
    observer_->OnIsPageTranslatedChanged(web_contents);
  }
}
