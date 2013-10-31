// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace content {
class WebContents;
}

class SearchIPCRouterTest;

// SearchIPCRouter is responsible for receiving and sending IPC messages between
// the browser and the Instant page.
class SearchIPCRouter : public content::WebContentsObserver {
 public:
  // SearchIPCRouter calls its delegate in response to messages received from
  // the page.
  class Delegate {
   public:
    // Called upon determination of Instant API support in response to the page
    // load event.
    virtual void OnInstantSupportDetermined(bool supports_instant) = 0;

    // Called upon determination of voice search API support.
    virtual void OnSetVoiceSearchSupport(bool supports_voice_search) = 0;

    // Called when the page wants the omnibox to be focused. |state| specifies
    // the omnibox focus state.
    virtual void FocusOmnibox(OmniboxFocusState state) = 0;

    // Called when the page wants to navigate to |url|. Usually used by the
    // page to navigate to privileged destinations (e.g. chrome:// URLs) or to
    // navigate to URLs that are hidden from the page using Restricted IDs (rid
    // in the API).
    virtual void NavigateToURL(const GURL& url,
                               WindowOpenDisposition disposition,
                               bool is_most_visited_item_url) = 0;

    // Called when the SearchBox wants to delete a Most Visited item.
    virtual void OnDeleteMostVisitedItem(const GURL& url) = 0;

    // Called when the SearchBox wants to undo a Most Visited deletion.
    virtual void OnUndoMostVisitedDeletion(const GURL& url) = 0;

    // Called when the SearchBox wants to undo all Most Visited deletions.
    virtual void OnUndoAllMostVisitedDeletions() = 0;

    // Called to signal that an event has occurred on the New Tab Page.
    virtual void OnLogEvent(NTPLoggingEventType event) = 0;

    // Called when the page wants to paste the |text| (or the clipboard contents
    // if the |text| is empty) into the omnibox.
    virtual void PasteIntoOmnibox(const string16& text) = 0;

    // Called when the SearchBox wants to verify the signed-in Chrome identity
    // against the provided |identity|. Will make a round-trip to the browser
    // and eventually return the result through SendChromeIdentityCheckResult.
    virtual void OnChromeIdentityCheck(const string16& identity) = 0;
  };

  // An interface to be implemented by consumers of SearchIPCRouter objects to
  // decide whether to process the message received from the page, and vice
  // versa (decide whether to send messages to the page).
  class Policy {
   public:
    virtual ~Policy() {}

    // SearchIPCRouter calls these functions before sending/receiving messages
    // to/from the page.
    virtual bool ShouldProcessSetVoiceSearchSupport() = 0;
    virtual bool ShouldProcessFocusOmnibox(bool is_active_tab) = 0;
    virtual bool ShouldProcessNavigateToURL(bool is_active_tab) = 0;
    virtual bool ShouldProcessDeleteMostVisitedItem() = 0;
    virtual bool ShouldProcessUndoMostVisitedDeletion() = 0;
    virtual bool ShouldProcessUndoAllMostVisitedDeletions() = 0;
    virtual bool ShouldProcessLogEvent() = 0;
    virtual bool ShouldProcessPasteIntoOmnibox(bool is_active_tab) = 0;
    virtual bool ShouldProcessChromeIdentityCheck() = 0;
    virtual bool ShouldSendSetPromoInformation() = 0;
    virtual bool ShouldSendSetDisplayInstantResults() = 0;
    virtual bool ShouldSendSetSuggestionToPrefetch() = 0;
    virtual bool ShouldSendMostVisitedItems() = 0;
    virtual bool ShouldSendThemeBackgroundInfo() = 0;
    virtual bool ShouldSubmitQuery() = 0;
  };

  SearchIPCRouter(content::WebContents* web_contents, Delegate* delegate,
                  scoped_ptr<Policy> policy);
  virtual ~SearchIPCRouter();

  // Tells the renderer to determine if the page supports the Instant API, which
  // results in a call to OnInstantSupportDetermined() when the reply is
  // received.
  void DetermineIfPageSupportsInstant();

  // Tells the renderer information it needs to display promos.
  void SetPromoInformation(bool is_app_launcher_enabled);

  // Tells the renderer whether to display the Instant results.
  void SetDisplayInstantResults();

  // Tells the renderer about the most visited items.
  void SendMostVisitedItems(const std::vector<InstantMostVisitedItem>& items);

  // Tells the renderer about the result of the Chrome identity check.
  void SendChromeIdentityCheckResult(const string16& identity,
                                     bool identity_match);

  // Tells the renderer about the current theme background.
  void SendThemeBackgroundInfo(const ThemeBackgroundInfo& theme_info);

  // Tells the page the suggestion to be prefetched if any.
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion);

  // Tells the page that the user pressed Enter in the omnibox.
  void Submit(const string16& text);

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

 private:
  friend class SearchIPCRouterTest;
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           DetermineIfPageSupportsInstant_Local);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           DetermineIfPageSupportsInstant_NonLocal);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           PageURLDoesntBelongToInstantRenderer);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessVoiceSearchSupportMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, ProcessLogEvent);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, DoNotProcessLogEvent);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessChromeIdentityCheck);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotProcessChromeIdentityCheck);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           SendSetDisplayInstantResults);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           SendSetSuggestionToPrefetch);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSendSetMessagesForIncognitoPage);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, SendMostVisitedItems);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSendMostVisitedItems);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, SendThemeBackgroundInfo);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSendThemeBackgroundInfo);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, SubmitQuery);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           AppropriateMessagesSentToIncognitoPages);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, ProcessFocusOmnibox);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, DoNotProcessFocusOmnibox);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, SendSetPromoInformation);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotSendSetPromoInformation);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest, ProcessNavigateToURL);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessDeleteMostVisitedItem);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessUndoMostVisitedDeletion);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessUndoAllMostVisitedDeletions);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           ProcessPasteIntoOmniboxMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotProcessPasteIntoOmniboxMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotProcessMessagesForIncognitoPage);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterPolicyTest,
                           DoNotProcessMessagesForInactiveTab);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, ProcessVoiceSearchSupportMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, IgnoreVoiceSearchSupportMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, ProcessFocusOmniboxMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, IgnoreFocusOmniboxMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, HandleTabChangedEvents);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, SendSetPromoInformationMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           DoNotSendSetPromoInformationMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, ProcessLogEventMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, IgnoreLogEventMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, ProcessChromeIdentityCheckMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, IgnoreChromeIdentityCheckMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, ProcessNavigateToURLMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, IgnoreNavigateToURLMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           ProcessDeleteMostVisitedItemMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           IgnoreDeleteMostVisitedItemMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           ProcessUndoMostVisitedDeletionMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           IgnoreUndoMostVisitedDeletionMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           ProcessUndoAllMostVisitedDeletionsMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           IgnoreUndoAllMostVisitedDeletionsMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           IgnoreMessageIfThePageIsNotActive);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, ProcessPasteAndOpenDropdownMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, IgnorePasteAndOpenDropdownMsg);

  // Overridden from contents::WebContentsObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnInstantSupportDetermined(int page_id, bool supports_instant) const;
  void OnVoiceSearchSupportDetermined(int page_id,
                                      bool supports_voice_search) const;
  void OnFocusOmnibox(int page_id, OmniboxFocusState state) const;
  void OnSearchBoxNavigate(int page_id,
                           const GURL& url,
                           WindowOpenDisposition disposition,
                           bool is_most_visited_item_url) const;
  void OnDeleteMostVisitedItem(int page_id, const GURL& url) const;
  void OnUndoMostVisitedDeletion(int page_id, const GURL& url) const;
  void OnUndoAllMostVisitedDeletions(int page_id) const;
  void OnLogEvent(int page_id, NTPLoggingEventType event) const;
  void OnPasteAndOpenDropDown(int page_id, const string16& text) const;
  void OnChromeIdentityCheck(int page_id, const string16& identity) const;

  // Used by unit tests to set a fake delegate.
  void set_delegate(Delegate* delegate);

  // Used by unit tests.
  void set_policy(scoped_ptr<Policy> policy);

  // Used by unit tests.
  Policy* policy() const { return policy_.get(); }

  Delegate* delegate_;
  scoped_ptr<Policy> policy_;

  // Set to true, when the tab corresponding to |this| instance is active.
  bool is_active_tab_;

  DISALLOW_COPY_AND_ASSIGN(SearchIPCRouter);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_H_
