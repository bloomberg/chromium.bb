// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_URL_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_URL_HANDLER_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// A structure to hold various URLs like devtools_page, homepage_url, etc
// that may be specified in the manifest of an extension.
struct ManifestURL : public Extension::ManifestData {
  GURL url_;

  // Returns the DevTools Page for this extension.
  static const GURL& GetDevToolsPage(const Extension* extension);

  // Returns the Homepage URL for this extension.
  // If homepage_url was not specified in the manifest,
  // this returns the Google Gallery URL. For third-party extensions,
  // this returns a blank GURL.
  static const GURL GetHomepageURL(const Extension* extension);
};

// Parses the "devtools_page" manifest key.
class DevToolsPageHandler : public ManifestHandler {
 public:
  DevToolsPageHandler();
  virtual ~DevToolsPageHandler();

  virtual bool Parse(const base::Value* value,
                     Extension* extension,
                     string16* error) OVERRIDE;
};

// Parses the "homepage_url" manifest key.
class HomepageURLHandler : public ManifestHandler {
 public:
  HomepageURLHandler();
  virtual ~HomepageURLHandler();

  virtual bool Parse(const base::Value* value,
                     Extension* extension,
                     string16* error) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_URL_HANDLER_H_
