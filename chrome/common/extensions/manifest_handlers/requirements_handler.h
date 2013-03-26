// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_REQUIREMENTS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_REQUIREMENTS_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// Declared requirements for the extension.
struct RequirementsInfo : public Extension::ManifestData {
  explicit RequirementsInfo(const Manifest* manifest);
  virtual ~RequirementsInfo();

  bool webgl;
  bool css3d;
  bool npapi;

  static const RequirementsInfo& GetRequirements(const Extension* extension);
};

// Parses the "requirements" manifest key.
class RequirementsHandler : public ManifestHandler {
 public:
  RequirementsHandler();
  virtual ~RequirementsHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

  virtual bool AlwaysParseForType(Manifest::Type type) const OVERRIDE;

  virtual const std::vector<std::string> PrerequisiteKeys() const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RequirementsHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_REQUIREMENTS_HANDLER_H_
