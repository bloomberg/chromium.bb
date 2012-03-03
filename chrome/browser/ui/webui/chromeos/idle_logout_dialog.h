// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IDLE_LOGOUT_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IDLE_LOGOUT_DIALOG_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/native_widget_types.h"

class IdleLogoutDialogHandler;

// A class to show the logout on idle dialog if the machine is in retail mode.
class IdleLogoutDialog : private HtmlDialogUIDelegate {
 public:
  static void ShowIdleLogoutDialog();
  static void CloseIdleLogoutDialog();

 private:
  IdleLogoutDialog();
  virtual ~IdleLogoutDialog();

  void ShowDialog();
  void CloseDialog();

  // HtmlDialogUIDelegate methods
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  content::WebContents* contents_;
  IdleLogoutDialogHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialog);
};

class IdleLogoutDialogUI : public HtmlDialogUI {
 public:
  explicit IdleLogoutDialogUI(content::WebUI* web_ui);
  virtual ~IdleLogoutDialogUI() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialogUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IDLE_LOGOUT_DIALOG_H_
