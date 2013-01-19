// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_INSTALL_EXTENSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_INSTALL_EXTENSION_HANDLER_H_

#include "base/file_path.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUIDataSource;
}

// Handles installing an extension when its file is dragged onto the page.
class InstallExtensionHandler : public content::WebUIMessageHandler {
 public:
  InstallExtensionHandler();
  virtual ~InstallExtensionHandler();

  void GetLocalizedValues(content::WebUIDataSource* source);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Handles a notification from the JavaScript that a drag has started. This is
  // needed so that we can capture the file being dragged. If we wait until
  // we receive a drop notification, the drop data in the browser process will
  // have already been destroyed.
  void HandleStartDragMessage(const ListValue* args);

  // Handles a notification from the JavaScript that a drag has stopped.
  void HandleStopDragMessage(const ListValue* args);

  // Handles a notification from the JavaScript to install the file currently
  // being dragged.
  //
  // IMPORTANT: We purposefully do not allow the JavaScript to specify the file
  // to be installed as a precaution against the extension management page
  // getting XSS'd.
  void HandleInstallMessage(const ListValue* args);

  // The extension that will be installed when HandleInstallMessage() is called.
  FilePath file_to_install_;

  DISALLOW_COPY_AND_ASSIGN(InstallExtensionHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_INSTALL_EXTENSION_HANDLER_H_
