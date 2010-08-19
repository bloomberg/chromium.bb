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
  RemotingSetupMessageHandler(RemotingSetupFlow* flow) : flow_(flow) {}
  virtual ~RemotingSetupMessageHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callbacks from the page.
  void HandleSubmitAuth(const ListValue* args);

  // These functions control which part of the HTML is visible.
  // TODO(hclam): I really don't feel right about exposing these as public
  // methods. It will be better that we notify RemotingSetupFlow and then get
  // a return value for state transition inside this class, or better we merge
  // this class into RemotingSetupFlow.
  void ShowGaiaSuccessAndSettingUp();
  void ShowGaiaFailed();
  void ShowSetupDone();

 private:
  void ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                 const std::wstring& js);
  RemotingSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(RemotingSetupMessageHandler);
};

#endif  // CHROME_BROWSER_REMOTING_REMOTING_SETUP_MESSAGE_HANDLER_H_
