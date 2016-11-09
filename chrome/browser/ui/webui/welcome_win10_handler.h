// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_WIN10_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_WIN10_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/common/shell_handler_win.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Handles actions on the Windows 10 specific Welcome page.
class WelcomeWin10Handler : public content::WebUIMessageHandler {
 public:
  WelcomeWin10Handler();
  ~WelcomeWin10Handler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Handlers for javascript calls.
  void HandleGetPinnedToTaskbarState(const base::ListValue* args);
  void HandleSetDefaultBrowser(const base::ListValue* args);
  void HandleContinue(const base::ListValue* args);

  void StartIsPinnedToTaskbarCheck();

  // Callback for mojom::ShellHandler's call to IsPinnedToTaskbar().
  void OnIsPinnedToTaskbarResult(bool succeeded, bool is_pinned_to_taskbar);

  // Sets the internal result and optionally call
  // SendPinnedToTaskbarStateResult() in the case that
  // |pinned_state_callback_id_| is not empty.
  void OnIsPinnedToTaskbarDetermined(bool is_pinned_to_taskbar);

  // Returns the result to the getPinnedToTaskbarState() javascript call via the
  // promise.
  void SendPinnedToTaskbarStateResult();

  std::unique_ptr<content::UtilityProcessMojoClient<mojom::ShellHandler>>
      client_;
  base::OneShotTimer timer_;

  // Acts as a cache to hold the taskbar pinned state of Chrome. It has no value
  // until this state is determined.
  base::Optional<bool> pinned_state_result_;

  // The callback id used to return the result to the getPinnedToTaskbarState()
  // javascript call. This id is empty until we receive the call; thus this
  // variable is used to determine if the result should be sent to the caller
  // when it is received, or wait for the call to happen.
  std::string pinned_state_callback_id_;

  DISALLOW_COPY_AND_ASSIGN(WelcomeWin10Handler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_WIN10_HANDLER_H_
