// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace {

// The maximum number of attempts we'll do to see if the page has finshed
// loading before giving up the translation
const int kMaxTranslateLoadCheckAttempts = 20;

}  // namespace

namespace translate {

ContentTranslateDriver::ContentTranslateDriver(
    content::NavigationController* nav_controller)
    : content::WebContentsObserver(nav_controller->GetWebContents()),
      navigation_controller_(nav_controller),
      translate_manager_(NULL),
      max_reload_check_attempts_(kMaxTranslateLoadCheckAttempts),
      weak_pointer_factory_(this) {
  DCHECK(navigation_controller_);
}

ContentTranslateDriver::~ContentTranslateDriver() {}

void ContentTranslateDriver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ContentTranslateDriver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ContentTranslateDriver::InitiateTranslation(const std::string& page_lang,
                                                 int attempt) {
  if (translate_manager_->GetLanguageState().translation_pending())
    return;

  // During a reload we need web content to be available before the
  // translate script is executed. Otherwise we will run the translate script on
  // an empty DOM which will fail. Therefore we wait a bit to see if the page
  // has finished.
  if (web_contents()->IsLoading() && attempt < max_reload_check_attempts_) {
    int backoff = attempt * kMaxTranslateLoadCheckAttempts;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ContentTranslateDriver::InitiateTranslation,
                   weak_pointer_factory_.GetWeakPtr(),
                   page_lang,
                   attempt + 1),
        base::TimeDelta::FromMilliseconds(backoff));
    return;
  }

  translate_manager_->InitiateTranslation(
      translate::TranslateDownloadManager::GetLanguageCode(page_lang));
}

// TranslateDriver methods

bool ContentTranslateDriver::IsLinkNavigation() {
  return navigation_controller_ && navigation_controller_->GetActiveEntry() &&
         navigation_controller_->GetActiveEntry()->GetTransitionType() ==
             ui::PAGE_TRANSITION_LINK;
}

void ContentTranslateDriver::OnTranslateEnabledChanged() {
  content::WebContents* web_contents = navigation_controller_->GetWebContents();
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnTranslateEnabledChanged(web_contents));
}

void ContentTranslateDriver::OnIsPageTranslatedChanged() {
    content::WebContents* web_contents =
        navigation_controller_->GetWebContents();
    FOR_EACH_OBSERVER(
        Observer, observer_list_, OnIsPageTranslatedChanged(web_contents));
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
                                ui::PAGE_TRANSITION_LINK,
                                false);
  navigation_controller_->GetWebContents()->OpenURL(params);
}

// content::WebContentsObserver methods

void ContentTranslateDriver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Check whether this is a reload: When doing a page reload, the
  // TranslateLanguageDetermined IPC is not sent so the translation needs to be
  // explicitly initiated.

  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  // If the navigation happened while offline don't show the translate
  // bar since there will be nothing to translate.
  if (load_details.http_status_code == 0 ||
      load_details.http_status_code == net::HTTP_INTERNAL_SERVER_ERROR) {
    return;
  }

  if (!load_details.is_main_frame &&
      translate_manager_->GetLanguageState().translation_declined()) {
    // Some sites (such as Google map) may trigger sub-frame navigations
    // when the user interacts with the page.  We don't want to show a new
    // infobar if the user already dismissed one in that case.
    return;
  }

  // If not a reload, return.
  if (entry->GetTransitionType() != ui::PAGE_TRANSITION_RELOAD &&
      load_details.type != content::NAVIGATION_TYPE_SAME_PAGE) {
    return;
  }

  if (!translate_manager_->GetLanguageState().page_needs_translation())
    return;

  // Note that we delay it as the ordering of the processing of this callback
  // by WebContentsObservers is undefined and might result in the current
  // infobars being removed. Since the translation initiation process might add
  // an infobar, it must be done after that.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ContentTranslateDriver::InitiateTranslation,
                 weak_pointer_factory_.GetWeakPtr(),
                 translate_manager_->GetLanguageState().original_language(),
                 0));
}

void ContentTranslateDriver::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Let the LanguageState clear its state.
  const bool reload =
      details.entry->GetTransitionType() == ui::PAGE_TRANSITION_RELOAD ||
      details.type == content::NAVIGATION_TYPE_SAME_PAGE;
  translate_manager_->GetLanguageState().DidNavigate(
      details.is_in_page, details.is_main_frame, reload);
}

bool ContentTranslateDriver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentTranslateDriver, message)
  IPC_MESSAGE_HANDLER(ChromeViewHostMsg_TranslateAssignedSequenceNumber,
                      OnTranslateAssignedSequenceNumber)
  IPC_MESSAGE_HANDLER(ChromeViewHostMsg_TranslateLanguageDetermined,
                      OnLanguageDetermined)
  IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PageTranslated, OnPageTranslated)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentTranslateDriver::OnTranslateAssignedSequenceNumber(
    int page_seq_no) {
  translate_manager_->set_current_seq_no(page_seq_no);
}

void ContentTranslateDriver::OnLanguageDetermined(
    const LanguageDetectionDetails& details,
    bool page_needs_translation) {
  translate_manager_->GetLanguageState().LanguageDetermined(
      details.adopted_language, page_needs_translation);

  if (web_contents())
    translate_manager_->InitiateTranslation(details.adopted_language);

  FOR_EACH_OBSERVER(Observer, observer_list_, OnLanguageDetermined(details));
}

void ContentTranslateDriver::OnPageTranslated(
    const std::string& original_lang,
    const std::string& translated_lang,
    TranslateErrors::Type error_type) {
  translate_manager_->PageTranslated(
      original_lang, translated_lang, error_type);
  FOR_EACH_OBSERVER(
      Observer,
      observer_list_,
      OnPageTranslated(original_lang, translated_lang, error_type));
}

}  // namespace translate
