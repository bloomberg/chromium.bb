// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// The implementation for the chrome://snippets-internals page.
class SnippetsInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public ntp_snippets::ContentSuggestionsService::Observer {
 public:
  SnippetsInternalsMessageHandler();
  ~SnippetsInternalsMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ntp_snippets::ContentSuggestionsService::Observer:
  void OnNewSuggestions() override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void ContentSuggestionsServiceShutdown() override;

  void HandleRefreshContent(const base::ListValue* args);
  void HandleClear(const base::ListValue* args);
  void HandleClearDismissed(const base::ListValue* args);
  void HandleDownload(const base::ListValue* args);
  void HandleClearCachedSuggestions(const base::ListValue* args);
  void HandleClearDismissedSuggestions(const base::ListValue* args);

  void SendAllContent();
  void SendSnippets();
  void SendDismissedSnippets();
  void SendHosts();
  void SendContentSuggestions();
  void SendBoolean(const std::string& name, bool value);
  void SendString(const std::string& name, const std::string& value);

  ScopedObserver<ntp_snippets::ContentSuggestionsService,
                 ntp_snippets::ContentSuggestionsService::Observer>
      content_suggestions_service_observer_;

  // Tracks whether we can already send messages to the page.
  bool dom_loaded_;

  ntp_snippets::NTPSnippetsService* ntp_snippets_service_;
  ntp_snippets::ContentSuggestionsService* content_suggestions_service_;

  DISALLOW_COPY_AND_ASSIGN(SnippetsInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_
