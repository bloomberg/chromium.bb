// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MOBILE_SETUP_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MOBILE_SETUP_DIALOG_H_
#pragma once

#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"

class MobileSetupDialog : public HtmlDialogUIDelegate {
 public:
  MobileSetupDialog();

  static void Show();
  static MobileSetupDialog* GetInstance();

 protected:
  friend struct DefaultSingletonTraits<MobileSetupDialog>;
  virtual ~MobileSetupDialog();

  void OnCloseDialog();

  // HtmlDialogUIDelegate overrides.
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

 private:
  void ShowDialog();

  DISALLOW_COPY_AND_ASSIGN(MobileSetupDialog);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MOBILE_SETUP_DIALOG_H_
