// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
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

  // Content Security Policy that should be used to enforce the sandbox used
  // by sandboxed pages (guaranteed to have the "sandbox" directive without the
  // "allow-same-origin" token).
  std::string sandbox_content_security_policy;

  static const std::string& GetContentSecurityPolicy(
      const Extension* extension);

  // Returns the extension's Content Security Policy for the sandboxed pages.
  static const std::string& GetSandboxContentSecurityPolicy(
      const Extension* extension);

  // Returns the Content Security Policy that the specified resource should be
  // served with.
  static const std::string& GetResourceContentSecurityPolicy(
      const Extension* extension,
      const std::string& relative_path);
};

// Parses "content_security_policy", "app.content_security_policy" and
// "sandbox.content_security_policy" manifest keys.
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
                              base::StringPiece manifest_key,
                              const base::Value* content_security_policy);

  // Parses the content security policy specified in the manifest for sandboxed
  // pages. This should be called after ParseExtensionPagesCSP.
  bool ParseSandboxCSP(Extension* extension,
                       base::string16* error,
                       base::StringPiece manifest_key,
                       const base::Value* sandbox_csp);

  // Sets the default CSP value for the extension.
  bool SetDefaultExtensionPagesCSP(Extension* extension);

  // Helper to set the sandbox content security policy manifest data.
  void SetSandboxCSP(Extension* extension, std::string sandbox_csp);

  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(CSPHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_CSP_INFO_H_
