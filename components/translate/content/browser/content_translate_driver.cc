// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include "base/logging.h"
#include "components/translate/content/common/translate_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace translate {

ContentTranslateDriver::ContentTranslateDriver(
    content::NavigationController* nav_controller)
    : navigation_controller_(nav_controller),
      observer_(NULL) {
  DCHECK(navigation_controller_);
}

ContentTranslateDriver::~ContentTranslateDriver() {}

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

void ContentTranslateDriver::TranslatePage(int page_seq_no,
                                           const std::string& translate_script,
                                           const std::string& source_lang,
                                           const std::string& target_lang) {
  content::WebContents* web_contents = navigation_controller_->GetWebContents();
  web_contents->GetRenderViewHost()->Send(
      new ChromeViewMsg_TranslatePage(
          web_contents->GetRenderViewHost()->GetRoutingID(),
          page_seq_no,
          translate_script,
          source_lang,
          target_lang));
}

void ContentTranslateDriver::RevertTranslation(int page_seq_no) {
  content::WebContents* web_contents = navigation_controller_->GetWebContents();
  web_contents->GetRenderViewHost()->Send(
      new ChromeViewMsg_RevertTranslation(
          web_contents->GetRenderViewHost()->GetRoutingID(),
          page_seq_no));
}

bool ContentTranslateDriver::IsOffTheRecord() {
  return navigation_controller_->GetBrowserContext()->IsOffTheRecord();
}

const std::string& ContentTranslateDriver::GetContentsMimeType() {
  return navigation_controller_->GetWebContents()->GetContentsMimeType();
}

const GURL& ContentTranslateDriver::GetLastCommittedURL() {
  return navigation_controller_->GetWebContents()->GetLastCommittedURL();
}

const GURL& ContentTranslateDriver::GetActiveURL() {
  content::NavigationEntry* entry = navigation_controller_->GetActiveEntry();
  if (!entry)
    return GURL::EmptyGURL();
  return entry->GetURL();
}

const GURL& ContentTranslateDriver::GetVisibleURL() {
  return navigation_controller_->GetWebContents()->GetVisibleURL();
}

bool ContentTranslateDriver::HasCurrentPage() {
  return (navigation_controller_->GetActiveEntry() != NULL);
}

void ContentTranslateDriver::OpenUrlInNewTab(const GURL& url) {
  content::OpenURLParams params(url,
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK,
                                false);
  navigation_controller_->GetWebContents()->OpenURL(params);
}

}  // namespace translate
