// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_SANDBOXED_PAGE_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_SANDBOXED_PAGE_INFO_H_

#include <string>

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

struct SandboxedPageInfo : public Extension::ManifestData {
 public:
  SandboxedPageInfo();
  virtual ~SandboxedPageInfo();

  // Returns the extension's Content Security Policy for the sandboxed pages.
  static const std::string& GetContentSecurityPolicy(
      const Extension* extension);

  // Returns the extension's sandboxed pages.
  static const URLPatternSet& GetPages(const Extension* extension);

  // Returns true if the specified page is sandboxed.
  static bool IsSandboxedPage(const Extension* extension,
                              const std::string& relative_path);

  // Optional list of extension pages that are sandboxed (served from a unique
  // origin with a different Content Security Policy).
  URLPatternSet pages;

  // Content Security Policy that should be used to enforce the sandbox used
  // by sandboxed pages (guaranteed to have the "sandbox" directive without the
  // "allow-same-origin" token).
  std::string content_security_policy;
};

class SandboxedPageHandler : public ManifestHandler {
 public:
  SandboxedPageHandler();
  virtual ~SandboxedPageHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_SANDBOXED_PAGE_INFO_H_
