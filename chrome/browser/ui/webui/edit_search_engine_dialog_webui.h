// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_WEBUI_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

class EditSearchEngineController;
class Profile;
class TemplateURL;
class EditSearchEngineDialogHandlerWebUI;

// EditSearchEngineDialogWebUI is the WebUI HTML dialog version of the edit
// search engine dialog.
class EditSearchEngineDialogWebUI : private HtmlDialogUIDelegate {
 public:
  static void ShowEditSearchEngineDialog(const TemplateURL* template_url,
                                         Profile* profile);

 private:
  explicit EditSearchEngineDialogWebUI(
      EditSearchEngineDialogHandlerWebUI* handler);

  // Shows the dialog
  void ShowDialog();

  // HtmlDialogUIDelegate methods
  virtual bool IsDialogModal() const OVERRIDE;
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

  // The message handler for this dialog.
  EditSearchEngineDialogHandlerWebUI* handler_;

  DISALLOW_COPY_AND_ASSIGN(EditSearchEngineDialogWebUI);
};

// EditSearchEngineDialogHandlerWebUI is the message handling component of the
// EditSearchEngineDialogWebUI. It handles messages from JavaScript, and it
// handles the closing of the dialog.
class EditSearchEngineDialogHandlerWebUI : public content::WebUIMessageHandler {
 public:
  EditSearchEngineDialogHandlerWebUI(const TemplateURL* template_url,
                                     Profile* profile);
  virtual ~EditSearchEngineDialogHandlerWebUI();

  // Overridden from WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

  // Returns true if adding, and false if editing, based on the template_url.
  bool IsAdding();

  // Although it's not really a message, the closing of the dialog is relevant
  // to the handler.
  void OnDialogClosed(const std::string& json_retval);

 private:
  void RequestDetails(const base::ListValue* args);
  void RequestValidation(const base::ListValue* args);

  // The template url.
  const TemplateURL* template_url_;

  // The profile.
  Profile* profile_;

  // The controller.
  scoped_ptr<EditSearchEngineController> controller_;

  DISALLOW_COPY_AND_ASSIGN(EditSearchEngineDialogHandlerWebUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_WEBUI_H_
