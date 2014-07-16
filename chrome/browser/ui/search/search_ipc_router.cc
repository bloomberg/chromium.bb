// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsProviderValid(const base::string16& provider) {
  // Only allow string of 8 alphanumeric characters or less as providers.
  // The empty string is considered valid and should be treated as if no
  // provider were specified.
  if (provider.length() > 8)
    return false;
  for (base::string16::const_iterator it = provider.begin();
       it != provider.end(); ++it) {
    if (!IsAsciiAlpha(*it) && !IsAsciiDigit(*it))
      return false;
  }
  return true;
}

}  // namespace

SearchIPCRouter::SearchIPCRouter(content::WebContents* web_contents,
                                 Delegate* delegate, scoped_ptr<Policy> policy)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      policy_(policy.Pass()),
      commit_counter_(0),
      is_active_tab_(false) {
  DCHECK(web_contents);
  DCHECK(delegate);
  DCHECK(policy_.get());
}

SearchIPCRouter::~SearchIPCRouter() {}

void SearchIPCRouter::OnNavigationEntryCommitted() {
  ++commit_counter_;
  Send(new ChromeViewMsg_SetPageSequenceNumber(routing_id(), commit_counter_));
}

void SearchIPCRouter::DetermineIfPageSupportsInstant() {
  Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
}

void SearchIPCRouter::SendChromeIdentityCheckResult(
    const base::string16& identity,
    bool identity_match) {
  if (!policy_->ShouldProcessChromeIdentityCheck())
    return;

  Send(new ChromeViewMsg_ChromeIdentityCheckResult(routing_id(), identity,
                                                   identity_match));
}

void SearchIPCRouter::SetPromoInformation(bool is_app_launcher_enabled) {
  if (!policy_->ShouldSendSetPromoInformation())
    return;

  Send(new ChromeViewMsg_SearchBoxPromoInformation(routing_id(),
                                                   is_app_launcher_enabled));
}

void SearchIPCRouter::SetDisplayInstantResults() {
  if (!policy_->ShouldSendSetDisplayInstantResults())
    return;

  bool is_search_results_page = !chrome::GetSearchTerms(web_contents()).empty();
  bool display_instant_results = is_search_results_page ?
      chrome::ShouldPrefetchSearchResultsOnSRP() :
          chrome::ShouldPrefetchSearchResults();
  Send(new ChromeViewMsg_SearchBoxSetDisplayInstantResults(
       routing_id(), display_instant_results));
}

void SearchIPCRouter::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  if (!policy_->ShouldSendSetSuggestionToPrefetch())
    return;

  Send(new ChromeViewMsg_SearchBoxSetSuggestionToPrefetch(routing_id(),
                                                          suggestion));
}

void SearchIPCRouter::SetOmniboxStartMargin(int start_margin) {
  if (!policy_->ShouldSendSetOmniboxStartMargin())
    return;

  Send(new ChromeViewMsg_SearchBoxMarginChange(routing_id(), start_margin));
}

void SearchIPCRouter::SetInputInProgress(bool input_in_progress) {
  if (!policy_->ShouldSendSetInputInProgress(is_active_tab_))
    return;

  Send(new ChromeViewMsg_SearchBoxSetInputInProgress(routing_id(),
                                                     input_in_progress));
}

void SearchIPCRouter::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  if (!policy_->ShouldSendOmniboxFocusChanged())
    return;

  Send(new ChromeViewMsg_SearchBoxFocusChanged(routing_id(), state, reason));
}

void SearchIPCRouter::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items) {
  if (!policy_->ShouldSendMostVisitedItems())
    return;

  Send(new ChromeViewMsg_SearchBoxMostVisitedItemsChanged(routing_id(), items));
}

void SearchIPCRouter::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  if (!policy_->ShouldSendThemeBackgroundInfo())
    return;

  Send(new ChromeViewMsg_SearchBoxThemeChanged(routing_id(), theme_info));
}

void SearchIPCRouter::ToggleVoiceSearch() {
  if (!policy_->ShouldSendToggleVoiceSearch())
    return;

  Send(new ChromeViewMsg_SearchBoxToggleVoiceSearch(routing_id()));
}

void SearchIPCRouter::Submit(const base::string16& text) {
  if (!policy_->ShouldSubmitQuery())
    return;

  Send(new ChromeViewMsg_SearchBoxSubmit(routing_id(), text));
}

void SearchIPCRouter::OnTabActivated() {
  is_active_tab_ = true;
}

void SearchIPCRouter::OnTabDeactivated() {
  is_active_tab_ = false;
}

bool SearchIPCRouter::OnMessageReceived(const IPC::Message& message) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!chrome::IsRenderedInInstantProcess(web_contents(), profile))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchIPCRouter, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetVoiceSearchSupported,
                        OnVoiceSearchSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FocusOmnibox, OnFocusOmnibox);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxNavigate,
                        OnSearchBoxNavigate);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem,
                        OnDeleteMostVisitedItem);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion,
                        OnUndoMostVisitedDeletion);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions,
                        OnUndoAllMostVisitedDeletions);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LogEvent, OnLogEvent);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LogMostVisitedImpression,
                        OnLogMostVisitedImpression);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LogMostVisitedNavigation,
                        OnLogMostVisitedNavigation);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PasteAndOpenDropdown,
                        OnPasteAndOpenDropDown);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ChromeIdentityCheck,
                        OnChromeIdentityCheck);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchIPCRouter::OnInstantSupportDetermined(int page_seq_no,
                                                 bool instant_support) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(instant_support);
}

void SearchIPCRouter::OnVoiceSearchSupportDetermined(
    int page_seq_no,
    bool supports_voice_search) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessSetVoiceSearchSupport())
    return;

  delegate_->OnSetVoiceSearchSupport(supports_voice_search);
}

void SearchIPCRouter::OnFocusOmnibox(int page_seq_no,
                                     OmniboxFocusState state) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessFocusOmnibox(is_active_tab_))
    return;

  delegate_->FocusOmnibox(state);
}

void SearchIPCRouter::OnSearchBoxNavigate(
    int page_seq_no,
    const GURL& url,
    WindowOpenDisposition disposition,
    bool is_most_visited_item_url) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessNavigateToURL(is_active_tab_))
    return;

  delegate_->NavigateToURL(url, disposition, is_most_visited_item_url);
}

void SearchIPCRouter::OnDeleteMostVisitedItem(int page_seq_no,
                                              const GURL& url) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessDeleteMostVisitedItem())
    return;

  delegate_->OnDeleteMostVisitedItem(url);
}

void SearchIPCRouter::OnUndoMostVisitedDeletion(int page_seq_no,
                                                const GURL& url) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessUndoMostVisitedDeletion())
    return;

  delegate_->OnUndoMostVisitedDeletion(url);
}

void SearchIPCRouter::OnUndoAllMostVisitedDeletions(int page_seq_no) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessUndoAllMostVisitedDeletions())
    return;

  delegate_->OnUndoAllMostVisitedDeletions();
}

void SearchIPCRouter::OnLogEvent(int page_seq_no,
                                 NTPLoggingEventType event) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogEvent(event);
}

void SearchIPCRouter::OnLogMostVisitedImpression(
    int page_seq_no, int position, const base::string16& provider) const {
  if (page_seq_no != commit_counter_ || !IsProviderValid(provider))
    return;

  delegate_->OnInstantSupportDetermined(true);
  // Logging impressions is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedImpression(position, provider);
}

void SearchIPCRouter::OnLogMostVisitedNavigation(
    int page_seq_no, int position, const base::string16& provider) const {
  if (page_seq_no != commit_counter_ || !IsProviderValid(provider))
    return;

  delegate_->OnInstantSupportDetermined(true);
  // Logging navigations is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedNavigation(position, provider);
}

void SearchIPCRouter::OnPasteAndOpenDropDown(int page_seq_no,
                                             const base::string16& text) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessPasteIntoOmnibox(is_active_tab_))
    return;

  delegate_->PasteIntoOmnibox(text);
}

void SearchIPCRouter::OnChromeIdentityCheck(
    int page_seq_no,
    const base::string16& identity) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessChromeIdentityCheck())
    return;

  delegate_->OnChromeIdentityCheck(identity);
}

void SearchIPCRouter::set_delegate_for_testing(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy_for_testing(scoped_ptr<Policy> policy) {
  DCHECK(policy.get());
  policy_.reset(policy.release());
}
