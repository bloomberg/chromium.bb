// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CSP_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_CSP_HANDLER_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// A structure to hold the Content-Security-Policy information.
struct CSPInfo : public Extension::ManifestData {
  explicit CSPInfo(const std::string& security_policy);
  virtual ~CSPInfo();

  // The Content-Security-Policy for an extension.  Extensions can use
  // Content-Security-Policies to mitigate cross-site scripting and other
  // vulnerabilities.
  std::string content_security_policy;

  static const std::string& GetContentSecurityPolicy(
      const Extension* extension);

  // Returns the Content Security Policy that the specified resource should be
  // served with.
  static const std::string& GetResourceContentSecurityPolicy(
      const Extension* extension,
      const std::string& relative_path);
};

// Parses "content_security_policy" and "app.content_security_policy" keys.
class CSPHandler : public ManifestHandler {
 public:
  explicit CSPHandler(bool is_platform_app);
  virtual ~CSPHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool AlwaysParseForType(Manifest::Type type) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  bool is_platform_app_;

  DISALLOW_COPY_AND_ASSIGN(CSPHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CSP_HANDLER_H_
