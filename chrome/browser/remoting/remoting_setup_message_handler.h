// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_REMOTING_SETUP_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_REMOTING_REMOTING_SETUP_MESSAGE_HANDLER_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"

class RemotingSetupFlow;

// This class is used to handle DOM messages from the setup dialog.
class RemotingSetupMessageHandler : public DOMMessageHandler {
 public:
  explicit RemotingSetupMessageHandler(RemotingSetupFlow* flow) : flow_(flow) {}
  virtual ~RemotingSetupMessageHandler() {}

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);

 protected:
  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callbacks from the page.
  void HandleSubmitAuth(const ListValue* args);

 private:
  RemotingSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(RemotingSetupMessageHandler);
};

#endif  // CHROME_BROWSER_REMOTING_REMOTING_SETUP_MESSAGE_HANDLER_H_
