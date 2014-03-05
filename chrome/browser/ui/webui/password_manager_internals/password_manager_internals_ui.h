// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_

#include "components/password_manager/core/browser/password_manager_logger.h"
#include "content/public/browser/web_ui_controller.h"

class PasswordManagerInternalsUI : public content::WebUIController,
                                   public PasswordManagerLogger {
 public:
  explicit PasswordManagerInternalsUI(content::WebUI* web_ui);
  virtual ~PasswordManagerInternalsUI();

  // PasswordManagerLogger implementation.
  virtual void LogSavePasswordProgress(const std::string& text) OVERRIDE;

 private:
  // These types describe which kinds of notifications
  // PasswordManagerInternalsUI can send to PasswordManagerClient.
  enum ClientNotificationType {
    PAGE_OPENED,  // Send when the page gets opened.
    PAGE_CLOSED   // Send when the page gets closed.
  };

  // This acts on all PasswordManagerClient instances of the current profile
  // based on |notification_type|:
  // PAGE_OPENED -- |this| is set as clients' PasswordManagerLogger
  // PAGE_CLOSED -- PasswordManagerLogger is reset for clients
  void NotifyAllPasswordManagerClients(
      ClientNotificationType notification_type);

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_
