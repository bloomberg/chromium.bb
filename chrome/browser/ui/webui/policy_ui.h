// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_reader.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "content/common/notification_observer.h"

// The base class handler of Javascript messages of the about:policy page.
class PolicyUIHandler : public WebUIMessageHandler {
 public:
  PolicyUIHandler();
  virtual ~PolicyUIHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  typedef policy::ConfigurationPolicyReader ConfigurationPolicyReader;

  // Callback for the "requestPolicyData" message.
  void HandleRequestPolicyData(const ListValue* args);

  scoped_ptr<policy::PolicyStatus> policy_status_;

  DISALLOW_COPY_AND_ASSIGN(PolicyUIHandler);
};

// The Web UI handler for about:policy.
class PolicyUI : public ChromeWebUI {
 public:
  explicit PolicyUI(TabContents* contents);
  virtual ~PolicyUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
