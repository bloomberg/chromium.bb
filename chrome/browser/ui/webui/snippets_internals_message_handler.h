// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/status.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace ntp_snippets {
class ContentSuggestionsService;
class ContextualContentSuggestionsService;
}  // namespace ntp_snippets

class PrefService;

// The implementation for the chrome://snippets-internals page.
class SnippetsInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public ntp_snippets::ContentSuggestionsService::Observer {
 public:
  SnippetsInternalsMessageHandler(
      ntp_snippets::ContentSuggestionsService* content_suggestions_service,
      ntp_snippets::ContextualContentSuggestionsService*
          contextual_content_suggestions_service,
      PrefService* pref_service);
  ~SnippetsInternalsMessageHandler() override;

 private:
  enum class DismissedState { HIDDEN, LOADING, VISIBLE };

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ntp_snippets::ContentSuggestionsService::Observer:
  void OnNewSuggestions(ntp_snippets::Category category) override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      const ntp_snippets::ContentSuggestion::ID& suggestion_id) override;
  void OnFullRefreshRequired() override;
  void ContentSuggestionsServiceShutdown() override;

  void HandleInitializationCompleted(const base::ListValue* args);
  void HandleRefreshContent(const base::ListValue* args);
  void HandleDownload(const base::ListValue* args);
  void HandleClearCachedSuggestions(const base::ListValue* args);
  void HandleClearDismissedSuggestions(const base::ListValue* args);
  void HandleToggleDismissedSuggestions(const base::ListValue* args);
  void HandleClearClassification(const base::ListValue* args);
  void HandleFetchRemoteSuggestionsInTheBackgroundIn2Seconds(
      const base::ListValue* args);
  void HandleFetchContextualSuggestions(const base::ListValue* args);
  void HandleResetNotificationsState(const base::ListValue* args);
  void OnContextualSuggestionsFetched(
      ntp_snippets::Status status_code,
      const GURL& url,
      std::vector<ntp_snippets::ContentSuggestion> suggestions);
  void HandlePushDummySuggestionIn10Seconds(const base::ListValue* args);

  void SendAllContent();
  void SendClassification();
  void SendRankerDebugData();
  void SendLastRemoteSuggestionsBackgroundFetchTime();
  void SendWhetherSuggestionPushingPossible();
  void SendContentSuggestions();
  void SendBoolean(const std::string& name, bool value);
  void SendString(const std::string& name, const std::string& value);

  void FetchRemoteSuggestionsInTheBackground();
  void PushDummySuggestion();

  void OnDismissedSuggestionsLoaded(
      ntp_snippets::Category category,
      std::vector<ntp_snippets::ContentSuggestion> dismissed_suggestions);

  ScopedObserver<ntp_snippets::ContentSuggestionsService,
                 ntp_snippets::ContentSuggestionsService::Observer>
      content_suggestions_service_observer_;

  // Tracks whether we can already send messages to the page.
  bool dom_loaded_;

  ntp_snippets::ContentSuggestionsService* content_suggestions_service_;
  ntp_snippets::ContextualContentSuggestionsService*
      contextual_content_suggestions_service_;
  ntp_snippets::RemoteSuggestionsProvider* remote_suggestions_provider_;
  PrefService* pref_service_;

  std::map<ntp_snippets::Category,
           DismissedState,
           ntp_snippets::Category::CompareByID>
      dismissed_state_;
  std::map<ntp_snippets::Category,
           std::vector<ntp_snippets::ContentSuggestion>,
           ntp_snippets::Category::CompareByID>
      dismissed_suggestions_;

  base::OneShotTimer suggestion_push_timer_;
  base::OneShotTimer suggestions_fetch_timer_;

  base::WeakPtrFactory<SnippetsInternalsMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SnippetsInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_
