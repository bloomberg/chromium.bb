// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_PACK_EXTENSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_PACK_EXTENSION_HANDLER_H_

#include <string>

#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/plugins/plugin_data_remover_helper.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebUIDataSource;
}

// Clear browser data handler page UI handler.
class PackExtensionHandler : public content::WebUIMessageHandler,
                             public ui::SelectFileDialog::Listener,
                             public extensions::PackExtensionJob::Client {
 public:
  PackExtensionHandler();
  virtual ~PackExtensionHandler();

  void GetLocalizedValues(content::WebUIDataSource* source);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // ExtensionPackJob::Client implementation.
  virtual void OnPackSuccess(const FilePath& crx_file,
                             const FilePath& key_file) OVERRIDE;

  virtual void OnPackFailure(const std::string& error,
                             extensions::ExtensionCreator::ErrorType) OVERRIDE;

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE {}

  // JavaScript callback to start packing an extension.
  void HandlePackMessage(const ListValue* args);

  // JavaScript callback to show a file browse dialog.
  // |args[0]| must be a string that specifies the file dialog type: file or
  // folder.
  // |args[1]| must be a string that specifies the operation to perform: load
  // or pem.
  void HandleSelectFilePathMessage(const ListValue* args);

  // A function to ask the page to show an alert.
  void ShowAlert(const std::string& message);

  // Used to package the extension.
  scoped_refptr<extensions::PackExtensionJob> pack_job_;

  // Returned by the SelectFileDialog machinery. Used to initiate the selection
  // dialog.
  scoped_refptr<ui::SelectFileDialog> load_extension_dialog_;

  // Path to root directory of extension
  std::string extension_path_;

  // Path to private key file, or null if none specified
  std::string private_key_path_;

  DISALLOW_COPY_AND_ASSIGN(PackExtensionHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_PACK_EXTENSION_HANDLER_H_
