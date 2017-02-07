// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "components/search/search.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/common/child_process_host.h"

namespace {

bool IsRenderedInInstantProcess(content::WebContents* web_contents) {
  // TODO(tibell): Instead of rejecting messages check at connection time if we
  // want to talk to the render frame or not. Later, in an out-of-process iframe
  // world, the IsRenderedInInstantProcess check will have to be done, as it's
  // based on the RenderView.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return search::IsRenderedInInstantProcess(web_contents, profile);
}

class SearchBoxClientFactoryImpl
    : public SearchIPCRouter::SearchBoxClientFactory {
 public:
  // The |web_contents| must outlive this object.
  explicit SearchBoxClientFactoryImpl(content::WebContents* web_contents)
      : web_contents_(web_contents),
        last_connected_rfh_(
            std::make_pair(content::ChildProcessHost::kInvalidUniqueID,
                           MSG_ROUTING_NONE)) {}
  chrome::mojom::SearchBox* GetSearchBox() override;

 private:
  void OnConnectionError();

  content::WebContents* web_contents_;
  chrome::mojom::SearchBoxAssociatedPtr search_box_;

  // The proccess ID and routing ID of the last connected main frame. May be
  // (ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE) if we haven't
  // connected yet.
  std::pair<int, int> last_connected_rfh_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxClientFactoryImpl);
};

chrome::mojom::SearchBox* SearchBoxClientFactoryImpl::GetSearchBox() {
  content::RenderFrameHost* frame = web_contents_->GetMainFrame();
  auto id = std::make_pair(frame->GetProcess()->GetID(), frame->GetRoutingID());
  // Avoid reconnecting repeatedly to the same RenderFrameHost for performance
  // reasons.
  if (id != last_connected_rfh_) {
    if (IsRenderedInInstantProcess(web_contents_)) {
      frame->GetRemoteAssociatedInterfaces()->GetInterface(&search_box_);
      search_box_.set_connection_error_handler(
          base::Bind(&SearchBoxClientFactoryImpl::OnConnectionError,
                     base::Unretained(this)));
    } else {
      // Renderer is not an instant process. We'll create a connection that
      // drops all messages.
      mojo::GetIsolatedProxy(&search_box_);
    }
    last_connected_rfh_ = id;
  }
  return search_box_.get();
}

void SearchBoxClientFactoryImpl::OnConnectionError() {
  search_box_.reset();
  last_connected_rfh_ = std::make_pair(
      content::ChildProcessHost::kInvalidUniqueID,
      MSG_ROUTING_NONE);
}

}  // namespace

SearchIPCRouter::SearchIPCRouter(content::WebContents* web_contents,
                                 Delegate* delegate,
                                 std::unique_ptr<Policy> policy)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      policy_(std::move(policy)),
      commit_counter_(0),
      is_active_tab_(false),
      bindings_(web_contents, this),
      search_box_client_factory_(new SearchBoxClientFactoryImpl(web_contents)) {
  DCHECK(web_contents);
  DCHECK(delegate);
  DCHECK(policy_.get());
}

SearchIPCRouter::~SearchIPCRouter() {}

void SearchIPCRouter::OnNavigationEntryCommitted() {
  ++commit_counter_;
  search_box()->SetPageSequenceNumber(commit_counter_);
}

void SearchIPCRouter::DetermineIfPageSupportsInstant() {
  search_box()->DetermineIfPageSupportsInstant();
}

void SearchIPCRouter::SendChromeIdentityCheckResult(
    const base::string16& identity,
    bool identity_match) {
  if (!policy_->ShouldProcessChromeIdentityCheck())
    return;

  search_box()->ChromeIdentityCheckResult(identity, identity_match);
}

void SearchIPCRouter::SendHistorySyncCheckResult(bool sync_history) {
  if (!policy_->ShouldProcessHistorySyncCheck())
    return;

  search_box()->HistorySyncCheckResult(sync_history);
}

void SearchIPCRouter::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  if (!policy_->ShouldSendSetSuggestionToPrefetch())
    return;

  search_box()->SetSuggestionToPrefetch(suggestion);
}

void SearchIPCRouter::SetInputInProgress(bool input_in_progress) {
  if (!policy_->ShouldSendSetInputInProgress(is_active_tab_))
    return;

  search_box()->SetInputInProgress(input_in_progress);
}

void SearchIPCRouter::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  if (!policy_->ShouldSendOmniboxFocusChanged())
    return;

  search_box()->FocusChanged(state, reason);
}

void SearchIPCRouter::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items) {
  if (!policy_->ShouldSendMostVisitedItems())
    return;

  search_box()->MostVisitedChanged(items);
}

void SearchIPCRouter::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  if (!policy_->ShouldSendThemeBackgroundInfo())
    return;

  search_box()->ThemeChanged(theme_info);
}

void SearchIPCRouter::Submit(const base::string16& text,
                             const EmbeddedSearchRequestParams& params) {
  if (!policy_->ShouldSubmitQuery())
    return;

  search_box()->Submit(text, params);
}

void SearchIPCRouter::OnTabActivated() {
  is_active_tab_ = true;
}

void SearchIPCRouter::OnTabDeactivated() {
  is_active_tab_ = false;
}

void SearchIPCRouter::InstantSupportDetermined(int page_seq_no,
                                               bool instant_support) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(instant_support);
}

void SearchIPCRouter::FocusOmnibox(int page_seq_no, OmniboxFocusState state) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessFocusOmnibox(is_active_tab_))
    return;

  delegate_->FocusOmnibox(state);
}

void SearchIPCRouter::SearchBoxDeleteMostVisitedItem(int page_seq_no,
                                                     const GURL& url) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessDeleteMostVisitedItem())
    return;

  delegate_->OnDeleteMostVisitedItem(url);
}

void SearchIPCRouter::SearchBoxUndoMostVisitedDeletion(int page_seq_no,
                                                       const GURL& url) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessUndoMostVisitedDeletion())
    return;

  delegate_->OnUndoMostVisitedDeletion(url);
}

void SearchIPCRouter::SearchBoxUndoAllMostVisitedDeletions(int page_seq_no) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessUndoAllMostVisitedDeletions())
    return;

  delegate_->OnUndoAllMostVisitedDeletions();
}

void SearchIPCRouter::LogEvent(int page_seq_no,
                               NTPLoggingEventType event,
                               base::TimeDelta time) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogEvent(event, time);
}

void SearchIPCRouter::LogMostVisitedImpression(
    int page_seq_no,
    int position,
    ntp_tiles::NTPTileSource tile_source) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  // Logging impressions is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedImpression(position, tile_source);
}

void SearchIPCRouter::LogMostVisitedNavigation(
    int page_seq_no,
    int position,
    ntp_tiles::NTPTileSource tile_source) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  // Logging navigations is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedNavigation(position, tile_source);
}

void SearchIPCRouter::PasteAndOpenDropdown(int page_seq_no,
                                           const base::string16& text) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessPasteIntoOmnibox(is_active_tab_))
    return;

  delegate_->PasteIntoOmnibox(text);
}

void SearchIPCRouter::ChromeIdentityCheck(int page_seq_no,
                                          const base::string16& identity) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessChromeIdentityCheck())
    return;

  delegate_->OnChromeIdentityCheck(identity);
}

void SearchIPCRouter::HistorySyncCheck(int page_seq_no) {
  if (!IsRenderedInInstantProcess(web_contents()))
    return;
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessHistorySyncCheck())
    return;

  delegate_->OnHistorySyncCheck();
}

void SearchIPCRouter::set_delegate_for_testing(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy_for_testing(std::unique_ptr<Policy> policy) {
  DCHECK(policy);
  policy_ = std::move(policy);
}
