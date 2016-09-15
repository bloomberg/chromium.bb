// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_FEEDBACK_MD_FEEDBACK_WEBUI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MD_FEEDBACK_MD_FEEDBACK_WEBUI_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

class MdFeedbackUI;

// The handler for Javascript messages related to the feedback dialog.
class MdFeedbackWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MdFeedbackWebUIMessageHandler(MdFeedbackUI* md_feedback_ui);
  ~MdFeedbackWebUIMessageHandler() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Handler for a JavaScript message that retrieves data (e.g. user email) to
  // populate the feedback form.
  void OnRequestData(const base::ListValue* args);

  MdFeedbackUI* md_feedback_ui_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(MdFeedbackWebUIMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_FEEDBACK_MD_FEEDBACK_WEBUI_MESSAGE_HANDLER_H_
