// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_LOADER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_LOADER_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace content {
class WebUIDataSource;
}

class Profile;

namespace extensions {

class Extension;

// The handler page for the Extension Commands UI overlay.
class ExtensionLoaderHandler : public content::WebUIMessageHandler {
 public:
  explicit ExtensionLoaderHandler(Profile* profile);
  virtual ~ExtensionLoaderHandler();

  // Fetches the localized values for the page and deposits them into |source|.
  void GetLocalizedValues(content::WebUIDataSource* source);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  class FileHelper;

  // Handle the 'extensionLoaderLoadUnpacked' message.
  void HandleLoadUnpacked(const base::ListValue* args);

  // Handle the 'extensionLoaderRetry' message.
  void HandleRetry(const base::ListValue* args);

  // Try to load an unpacked extension from the given |file_path|.
  void LoadUnpackedExtensionImpl(const base::FilePath& file_path);

  // Called when an unpacked extension fails to load.
  void OnLoadFailure(const base::FilePath& file_path, const std::string& error);

  // Notify the frontend of the failure. If it was a manifest error, |manifest|
  // will hold the manifest contents, and |line_number| will point to the line
  // at which the error was found.
  void NotifyFrontendOfFailure(const base::FilePath& file_path,
                               const std::string& error,
                               size_t line_number,
                               const std::string& manifest);

  // The profile with which this Handler is associated.
  Profile* profile_;

  // A helper to manage file picking.
  scoped_ptr<FileHelper> file_helper_;

  // The file path to the extension that failed to load, or empty. This is
  // loaded when the user selects "retry".
  base::FilePath failed_path_;

  // Weak pointer factory for posting background tasks.
  base::WeakPtrFactory<ExtensionLoaderHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionLoaderHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_LOADER_HANDLER_H_
