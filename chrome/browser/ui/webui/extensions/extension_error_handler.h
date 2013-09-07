// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ERROR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ERROR_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class WebUIDataSource;
}

class Profile;

namespace extensions {

class Extension;

// The handler page for the Extension Commands UI overlay.
class ExtensionErrorHandler : public content::WebUIMessageHandler {
 public:
  explicit ExtensionErrorHandler(Profile* profile);
  virtual ~ExtensionErrorHandler();

  // Fetches the localized values for the page and deposits them into |source|.
  void GetLocalizedValues(content::WebUIDataSource* source);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  friend class ManifestHighlightUnitTest;

  // Handle the "requestFileSource" call.
  void HandleRequestFileSource(const base::ListValue* args);

  // Populate the results for a manifest file's content in response to the
  // "requestFileSource" call. Highlight the part of the manifest which
  // corresponds to the given |key| and |specific| locations. Caller owns
  // |dict| and |contents|.
  void GetManifestFileCallback(base::DictionaryValue* dict,
                               const std::string& key,
                               const std::string& specific,
                               std::string* contents);

  // Populate the results for a source file's content in response to the
  // "requestFileSource" call. Highlight the part of the source which
  // corresponds to the given |line_number|.
  void GetSourceFileCallback(base::DictionaryValue* results,
                             int line_number,
                             std::string* contents);

  // The profile with which this Handler is associated.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionErrorHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ERROR_HANDLER_H_
