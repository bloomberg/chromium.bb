// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_CONTENT_SCRIPTS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_CONTENT_SCRIPTS_HANDLER_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/user_script.h"

namespace extensions {

struct ContentScriptsInfo : public Extension::ManifestData {
  ContentScriptsInfo();
  virtual ~ContentScriptsInfo();

  // Paths to the content scripts the extension contains (possibly empty).
  UserScriptList content_scripts;

  // Returns the content scripts for the extension (if the extension has
  // no content scripts, this returns an empty list).
  static const UserScriptList& GetContentScripts(const Extension* extension);

  // Returns true if the extension has a content script declared at |url|.
  static bool ExtensionHasScriptAtURL(const Extension* extension,
                                      const GURL& url);
};

// Parses the "content_scripts" manifest key.
class ContentScriptsHandler : public ManifestHandler {
 public:
  ContentScriptsHandler();
  virtual ~ContentScriptsHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ContentScriptsHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_CONTENT_SCRIPTS_HANDLER_H_
