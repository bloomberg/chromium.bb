// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// The implementation for the chrome://snippets-internals page.
class SnippetsInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public ntp_snippets::NTPSnippetsServiceObserver {
 public:
  SnippetsInternalsMessageHandler();
  ~SnippetsInternalsMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ntp_snippets::NTPSnippetsServiceObserver:
  // Send everytime the service loads a new set of data.
  void NTPSnippetsServiceLoaded() override;
  // Send when the service is shutting down.
  void NTPSnippetsServiceShutdown() override;

  void HandleLoaded(const base::ListValue* args);
  void HandleClear(const base::ListValue* args);
  void HandleDump(const base::ListValue* args);
  void HandleClearDiscarded(const base::ListValue* args);
  void HandleDownload(const base::ListValue* args);

  void SendInitialData();
  void SendSnippets();
  void SendDiscardedSnippets();
  void SendHosts();
  void SendJson(const std::string& json);
  void SendBoolean(const std::string& name, bool value);
  void SendString(const std::string& name, const std::string& value);

  ScopedObserver<ntp_snippets::NTPSnippetsService,
                 ntp_snippets::NTPSnippetsServiceObserver> observer_;
  bool dom_loaded_;
  ntp_snippets::NTPSnippetsService* ntp_snippets_service_;

  DISALLOW_COPY_AND_ASSIGN(SnippetsInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_MESSAGE_HANDLER_H_
