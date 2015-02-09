// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_builder.h"

#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"

namespace sessions {

// static
SerializedNavigationEntry
ContentSerializedNavigationBuilder::FromNavigationEntry(
    int index,
    const content::NavigationEntry& entry) {
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = entry.GetUniqueID();
  navigation.referrer_url_ = entry.GetReferrer().url;
  navigation.referrer_policy_ = entry.GetReferrer().policy;
  navigation.virtual_url_ = entry.GetVirtualURL();
  navigation.title_ = entry.GetTitle();
  navigation.encoded_page_state_ = entry.GetPageState().ToEncodedData();
  navigation.transition_type_ = entry.GetTransitionType();
  navigation.has_post_data_ = entry.GetHasPostData();
  navigation.post_id_ = entry.GetPostID();
  navigation.original_request_url_ = entry.GetOriginalRequestURL();
  navigation.is_overriding_user_agent_ = entry.GetIsOverridingUserAgent();
  navigation.timestamp_ = entry.GetTimestamp();
  navigation.is_restored_ = entry.IsRestored();
  // If you want to navigate a named frame in Chrome, you will first need to
  // add support for persisting it. It is currently only used for layout tests.
  CHECK(entry.GetFrameToNavigate().empty());
  entry.GetExtraData(kSearchTermsKey, &navigation.search_terms_);
  if (entry.GetFavicon().valid)
    navigation.favicon_url_ = entry.GetFavicon().url;
  navigation.http_status_code_ = entry.GetHttpStatusCode();
  navigation.redirect_chain_ = entry.GetRedirectChain();

  return navigation;
}

// static
scoped_ptr<content::NavigationEntry>
ContentSerializedNavigationBuilder::ToNavigationEntry(
    const SerializedNavigationEntry* navigation,
    int page_id,
    content::BrowserContext* browser_context) {
  blink::WebReferrerPolicy policy =
      static_cast<blink::WebReferrerPolicy>(navigation->referrer_policy_);
  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationController::CreateNavigationEntry(
          navigation->virtual_url_,
          content::Referrer::SanitizeForRequest(
              navigation->virtual_url_,
              content::Referrer(navigation->referrer_url_, policy)),
          // Use a transition type of reload so that we don't incorrectly
          // increase the typed count.
          ui::PAGE_TRANSITION_RELOAD, false,
          // The extra headers are not sync'ed across sessions.
          std::string(), browser_context));

  entry->SetTitle(navigation->title_);
  entry->SetPageState(content::PageState::CreateFromEncodedData(
      navigation->encoded_page_state_));
  entry->SetPageID(page_id);
  entry->SetHasPostData(navigation->has_post_data_);
  entry->SetPostID(navigation->post_id_);
  entry->SetOriginalRequestURL(navigation->original_request_url_);
  entry->SetIsOverridingUserAgent(navigation->is_overriding_user_agent_);
  entry->SetTimestamp(navigation->timestamp_);
  entry->SetExtraData(kSearchTermsKey, navigation->search_terms_);
  entry->SetHttpStatusCode(navigation->http_status_code_);
  entry->SetRedirectChain(navigation->redirect_chain_);

  // These fields should have default values.
  DCHECK_EQ(SerializedNavigationEntry::STATE_INVALID,
            navigation->blocked_state_);
  DCHECK_EQ(0u, navigation->content_pack_categories_.size());

  return entry.Pass();
}

// static
ScopedVector<content::NavigationEntry>
ContentSerializedNavigationBuilder::ToNavigationEntries(
    const std::vector<SerializedNavigationEntry>& navigations,
    content::BrowserContext* browser_context) {
  int page_id = 0;
  ScopedVector<content::NavigationEntry> entries;
  for (std::vector<SerializedNavigationEntry>::const_iterator
       it = navigations.begin(); it != navigations.end(); ++it) {
    entries.push_back(
        ToNavigationEntry(&(*it), page_id, browser_context).release());
    ++page_id;
  }
  return entries.Pass();
}

}  // namespace sessions
