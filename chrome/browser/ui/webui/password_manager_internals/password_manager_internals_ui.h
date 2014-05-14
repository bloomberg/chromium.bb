// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_

#include "components/password_manager/core/browser/password_manager_logger.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"

class PasswordManagerInternalsUI
    : public content::WebUIController,
      public content::WebContentsObserver,
      public password_manager::PasswordManagerLogger {
 public:
  explicit PasswordManagerInternalsUI(content::WebUI* web_ui);
  virtual ~PasswordManagerInternalsUI();

  // WebContentsObserver implementation.
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // PasswordManagerLogger implementation.
  virtual void LogSavePasswordProgress(const std::string& text) OVERRIDE;

 private:
  // Whether |this| registered as a log receiver with the
  // PasswordManagerInternalsService.
  bool registered_with_logging_service_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_
