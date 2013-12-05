// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_WEBVIEW_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_WEBVIEW_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the <webview> accessible extension resources
// that may be specified in the manifest of an extension using the
// "webview" key.
struct WebviewInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  WebviewInfo();
  virtual ~WebviewInfo();

  // Returns true if the specified resource is web accessible.
  static bool IsResourceWebviewAccessible(const Extension* extension,
                                          const std::string& partition_id,
                                          const std::string& relative_path);

  // Returns true when 'webview_accessible_resources' are defined for the
  // app.
  static bool HasWebviewAccessibleResources(const Extension* extension);

  // Optional list of webview accessible extension resources.
  std::vector<std::string> webview_privileged_partitions_;
  URLPatternSet webview_accessible_resources_;
};

// Parses the "webview" manifest key.
class WebviewHandler : public ManifestHandler {
 public:
  WebviewHandler();
  virtual ~WebviewHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WebviewHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_WEBVIEW_HANDLER_H_
