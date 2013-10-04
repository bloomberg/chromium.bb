// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/web_contents.h"

SearchIPCRouter::SearchIPCRouter(content::WebContents* web_contents,
                                 Delegate* delegate, scoped_ptr<Policy> policy)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      policy_(policy.Pass()) {
  DCHECK(web_contents);
  DCHECK(delegate);
  DCHECK(policy_.get());
}

SearchIPCRouter::~SearchIPCRouter() {}

void SearchIPCRouter::DetermineIfPageSupportsInstant() {
  Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
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

  Send(new ChromeViewMsg_SearchBoxSetDisplayInstantResults(
       routing_id(),
       chrome::ShouldPrefetchSearchResultsOnSRP()));
}

void SearchIPCRouter::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  if (!policy_->ShouldSendThemeBackgroundInfo())
    return;

  Send(new ChromeViewMsg_SearchBoxThemeChanged(routing_id(), theme_info));
}

void SearchIPCRouter::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items) {
  if (!policy_->ShouldSendMostVisitedItems())
    return;

  Send(new ChromeViewMsg_SearchBoxMostVisitedItemsChanged(
      routing_id(), items));
}

void SearchIPCRouter::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  if (!policy_->ShouldSendSetSuggestionToPrefetch())
    return;

  Send(new ChromeViewMsg_SearchBoxSetSuggestionToPrefetch(routing_id(),
                                                          suggestion));
}

void SearchIPCRouter::Submit(const string16& text) {
  if (!policy_->ShouldSubmitQuery())
    return;

  Send(new ChromeViewMsg_SearchBoxSubmit(routing_id(), text));
}

bool SearchIPCRouter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchIPCRouter, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetVoiceSearchSupported,
                        OnVoiceSearchSupportDetermined)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchIPCRouter::OnInstantSupportDetermined(int page_id,
                                                 bool instant_support) const {
  if (!web_contents()->IsActiveEntry(page_id))
    return;

  delegate_->OnInstantSupportDetermined(instant_support);
}

void SearchIPCRouter::OnVoiceSearchSupportDetermined(
    int page_id,
    bool supports_voice_search) const {
  if (!web_contents()->IsActiveEntry(page_id) ||
      !policy_->ShouldProcessSetVoiceSearchSupport())
    return;

  delegate_->OnSetVoiceSearchSupport(supports_voice_search);
}

void SearchIPCRouter::set_delegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy(scoped_ptr<Policy> policy) {
  DCHECK(policy.get());
  policy_.reset(policy.release());
}
