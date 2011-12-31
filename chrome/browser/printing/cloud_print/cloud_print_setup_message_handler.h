// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_MESSAGE_HANDLER_H_

#include <string>

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class CloudPrintSetupFlow;

// This class is used to handle DOM messages from the setup dialog.
class CloudPrintSetupMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit CloudPrintSetupMessageHandler(CloudPrintSetupFlow* flow)
    : flow_(flow) {}
  virtual ~CloudPrintSetupMessageHandler() {}

 protected:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callbacks from the page.
  void HandleSubmitAuth(const ListValue* args);
  void HandlePrintTestPage(const ListValue* args);
  void HandleLearnMore(const ListValue* args);

 private:
  CloudPrintSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintSetupMessageHandler);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_MESSAGE_HANDLER_H_
