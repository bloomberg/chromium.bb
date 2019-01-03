// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_

#include <string>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the Content-Security-Policy information.
struct CSPInfo : public Extension::ManifestData {
  explicit CSPInfo(const std::string& security_policy);
  ~CSPInfo() override;

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
  CSPHandler();
  ~CSPHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;
  bool AlwaysParseForType(Manifest::Type type) const override;

 private:
  // Parses the "content_security_policy" dictionary in the manifest.
  bool ParseCSPDictionary(Extension* extension,
                          base::string16* error,
                          const base::Value& csp_dict);

  // Parses the content security policy specified in the manifest for extension
  // pages.
  bool ParseExtensionPagesCSP(Extension* extension,
                              base::string16* error,
                              const std::string& manifest_key,
                              const base::Value* content_security_policy);

  // Sets the default CSP value for the extension.
  bool SetDefaultExtensionPagesCSP(Extension* extension);

  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(CSPHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
