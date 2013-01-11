// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_WEB_INTENTS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_WEB_INTENTS_HANDLER_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace webkit_glue {
struct WebIntentServiceData;
}

namespace extensions {

typedef std::vector<webkit_glue::WebIntentServiceData> WebIntentServiceDataList;

// A structure to hold the parsed web intent service data list
// that may be specified in the manifest of an extension using
// "intents" key.
struct WebIntentsInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  WebIntentsInfo();
  virtual ~WebIntentsInfo();

  static const WebIntentServiceDataList& GetIntentsServices(
      const Extension* extension);

  // List of intents services that the extension provides, if any.
  WebIntentServiceDataList intents_services_;
};

// Parses the "intents" manifest key.
class WebIntentsHandler : public ManifestHandler {
 public:
  WebIntentsHandler();
  virtual ~WebIntentsHandler();

  virtual bool Parse(const base::Value* value,
                     Extension* extension,
                     string16* error) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_WEB_INTENTS_HANDLER_H_
