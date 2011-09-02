// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/native_widget_types.h"

class TabContents;

class HungRendererDialog : private HtmlDialogUIDelegate {
 public:
  HungRendererDialog();

  // Shows the hung renderer dialog.
  void ShowDialog(gfx::NativeWindow owning_window, TabContents* contents);

 private:
  // HtmlDialogUIDelegate methods
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

  // The tab contents.
  TabContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialog);
};

// Dialog handler that handles calls from the JS WebUI code to get the details
// of the list of frozen tabs.
class HungRendererDialogHandler : public WebUIMessageHandler {
 public:
  explicit HungRendererDialogHandler(TabContents* contents);

  // Overridden from WebUIMessageHandler
  virtual void RegisterMessages();

 private:
  void RequestTabContentsList(const base::ListValue* args);

  // The tab contents.
  TabContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogHandler);
};


#endif  // CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_H_
