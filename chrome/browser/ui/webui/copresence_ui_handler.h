// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COPRESENCE_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_COPRESENCE_UI_HANDLER_H_

#include "base/macros.h"
#include "components/copresence/public/copresence_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

namespace copresence {
class CopresenceState;
}

// UI Handler for the copresence page.
// TODO(crbug.com/444263): Write tests.
class CopresenceUIHandler final : public content::WebUIMessageHandler,
                                  public copresence::CopresenceObserver {
 public:
  explicit CopresenceUIHandler(content::WebUI* web_ui);
  ~CopresenceUIHandler() override;

 private:
  // WebUIMessageHandler override.
  void RegisterMessages() override;

  // CopresenceObserver overrides.
  void DirectivesUpdated() override;
  void TokenTransmitted(const copresence::TransmittedToken& token) override;
  void TokenReceived(const copresence::ReceivedToken& token) override;

  // Handler to populate the page with its initial state.
  void HandlePopulateState(const base::ListValue* args);

  // Handler for the clear state button.
  void HandleClearState(const base::ListValue* args);

  copresence::CopresenceState* state_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COPRESENCE_UI_HANDLER_H_
