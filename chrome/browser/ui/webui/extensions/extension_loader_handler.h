// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_LOADER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_LOADER_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUIDataSource;
}

class Profile;

namespace extensions {

class Extension;

// The handler page for the Extension Commands UI overlay.
class ExtensionLoaderHandler : public content::WebUIMessageHandler,
                               public ExtensionErrorReporter::Observer,
                               public content::WebContentsObserver {
 public:
  explicit ExtensionLoaderHandler(Profile* profile);
  ~ExtensionLoaderHandler() override;

  // Fetches the localized values for the page and deposits them into |source|.
  void GetLocalizedValues(content::WebUIDataSource* source);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Handle the 'extensionLoaderRetry' message.
  void HandleRetry(const base::ListValue* args);

  // Handle the 'extensionLoaderIgnoreFailure' message.
  void HandleIgnoreFailure(const base::ListValue* args);

  // Handle the 'extensionLoaderDisplayFailures' message.
  void HandleDisplayFailures(const base::ListValue* args);

  // Try to load an unpacked extension from the given |file_path|.
  void LoadUnpackedExtension(const base::FilePath& file_path);

  // ExtensionErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& file_path,
                     const std::string& error) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Add a failure to |failures_|. If it was a manifest error, |manifest| will
  // hold the manifest contents, and |line_number| will point to the line at
  // which the error was found.
  void AddFailure(const base::FilePath& file_path,
                  const std::string& error,
                  size_t line_number,
                  const std::string& manifest);

  // Notify the frontend of all failures.
  void NotifyFrontendOfFailure();

  // The profile with which this Handler is associated.
  Profile* profile_;

  // Holds information about all unpacked extension install failures that
  // were reported while the extensions page was loading.
  base::ListValue failures_;

  // Holds failed paths for load retries.
  std::vector<base::FilePath> failed_paths_;

  ScopedObserver<ExtensionErrorReporter, ExtensionErrorReporter::Observer>
      extension_error_reporter_observer_;

  // Set when the chrome://extensions page is fully loaded and the frontend is
  // ready to receive failure notifications. We need this because the page
  // fails to display failures if they are sent before the Javascript is loaded.
  bool ui_ready_;

  // Weak pointer factory for posting background tasks.
  base::WeakPtrFactory<ExtensionLoaderHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionLoaderHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_LOADER_HANDLER_H_
