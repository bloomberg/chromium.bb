// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_ENROLLMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_ENROLLMENT_UI_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/webui/web_ui.h"

namespace chromeos {

// WebUI implementation that handles the enterprise enrollment dialog in the
// Chrome OS login flow.
class EnterpriseEnrollmentUI : public WebUI {
 public:
  // This defines the interface for controllers which will be called back when
  // something happens on the UI. It is stored in a property of the TabContents.
  class Controller {
   public:
    virtual ~Controller() {}

    virtual void OnAuthSubmitted(const std::string& user,
                                 const std::string& password,
                                 const std::string& captcha,
                                 const std::string& access_code) = 0;
    virtual void OnAuthCancelled() = 0;
    virtual void OnConfirmationClosed() = 0;
    virtual bool GetInitialUser(std::string* user) = 0;
  };

  explicit EnterpriseEnrollmentUI(TabContents* contents);
  virtual ~EnterpriseEnrollmentUI();

  // Overriden from WebUI.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

  // Gets the URL for loading the UI.
  static GURL GetURL();

  // Gets the controller the given |web_ui| is associated with. Returns NULL if
  // there is no controller set.
  static Controller* GetController(WebUI* web_ui);

  // Sets the controller on a tab contents.
  static void SetController(TabContents* contents, Controller* controller);

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_ENROLLMENT_UI_H_
