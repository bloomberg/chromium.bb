// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include <stddef.h>
#include <utility>

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

bool IsContentsFrom(const InstantTab* page,
                    const content::WebContents* contents) {
  return page && (page->web_contents() == contents);
}

// Adds a transient NavigationEntry to the supplied |contents|'s
// NavigationController if the page's URL has not already been updated with the
// supplied |search_terms|. Sets the |search_terms| on the transient entry for
// search terms extraction to work correctly.
void EnsureSearchTermsAreSet(content::WebContents* contents,
                             const base::string16& search_terms) {
  content::NavigationController* controller = &contents->GetController();

  // If search terms are already correct or there is already a transient entry
  // (there shouldn't be), bail out early.
  if (search::GetSearchTerms(contents) == search_terms ||
      controller->GetTransientEntry())
    return;

  const content::NavigationEntry* entry = controller->GetLastCommittedEntry();
  std::unique_ptr<content::NavigationEntry> transient =
      controller->CreateNavigationEntry(
          entry->GetURL(), entry->GetReferrer(), entry->GetTransitionType(),
          false, std::string(), contents->GetBrowserContext());
  transient->SetExtraData(sessions::kSearchTermsKey, search_terms);
  controller->SetTransientEntry(std::move(transient));

  SearchTabHelper::FromWebContents(contents)->NavigationEntryUpdated();
}

}  // namespace

InstantController::InstantController(BrowserInstantController* browser)
    : browser_(browser) {
}

InstantController::~InstantController() {
}

bool InstantController::SubmitQuery(const base::string16& search_terms,
                                    const EmbeddedSearchRequestParams& params) {
  if (!instant_tab_ || !instant_tab_->web_contents())
    return false;

  SearchTabHelper* search_tab =
      SearchTabHelper::FromWebContents(instant_tab_->web_contents());
  if (!search_tab->SupportsInstant() || !search_mode_.is_origin_search())
    return false;

  // Use |instant_tab_| to run the query if we're already on a search results
  // page. (NOTE: in particular, we do not send the query to NTPs.)
  search_tab->Submit(search_terms, params);
  instant_tab_->web_contents()->Focus();
  EnsureSearchTermsAreSet(instant_tab_->web_contents(), search_terms);
  return true;
}

void InstantController::SearchModeChanged(const SearchMode& old_mode,
                                          const SearchMode& new_mode) {
  LogDebugEvent(base::StringPrintf(
      "SearchModeChanged: [origin:mode] %d:%d to %d:%d", old_mode.origin,
      old_mode.mode, new_mode.origin, new_mode.mode));

  search_mode_ = new_mode;
  ResetInstantTab();
}

void InstantController::ActiveTabChanged() {
  LogDebugEvent("ActiveTabChanged");
  ResetInstantTab();
}

void InstantController::LogDebugEvent(const std::string& info) const {
  DVLOG(1) << info;

  debug_events_.push_front(std::make_pair(
      base::Time::Now().ToInternalValue(), info));
  static const size_t kMaxDebugEventSize = 2000;
  if (debug_events_.size() > kMaxDebugEventSize)
    debug_events_.pop_back();
}

void InstantController::ClearDebugEvents() {
  debug_events_.clear();
}

void InstantController::InstantSupportChanged(
    InstantSupportState instant_support) {
  // Handle INSTANT_SUPPORT_YES here because InstantTab is not hooked up to the
  // active tab. Search model changed listener in InstantTab will handle other
  // cases.
  if (instant_support != INSTANT_SUPPORT_YES)
    return;

  ResetInstantTab();
}

void InstantController::InstantSupportDetermined(
    const content::WebContents* contents,
    bool supports_instant) {
  DCHECK(IsContentsFrom(instant_tab_.get(), contents));

  if (!supports_instant)
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    instant_tab_.release());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

void InstantController::InstantTabAboutToNavigateMainFrame(
    const content::WebContents* contents,
    const GURL& url) {
  DCHECK(IsContentsFrom(instant_tab_.get(), contents));

  // The Instant tab navigated.  Send it the data it needs to display
  // properly.
  UpdateInfoForInstantTab();
}

void InstantController::ResetInstantTab() {
  if (!search_mode_.is_origin_default()) {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    if (!instant_tab_ || active_tab != instant_tab_->web_contents()) {
      instant_tab_.reset(new InstantTab(this, active_tab));
      instant_tab_->Init();
      UpdateInfoForInstantTab();
    }
  } else {
    instant_tab_.reset();
  }
}

void InstantController::UpdateInfoForInstantTab() {
  if (instant_tab_) {
    // Update theme details.
    InstantService* instant_service = GetInstantService();
    if (instant_service) {
      instant_service->UpdateThemeInfo();
      instant_service->UpdateMostVisitedItemsInfo();
    }
  }
}

InstantService* InstantController::GetInstantService() const {
  return InstantServiceFactory::GetForProfile(browser_->profile());
}
