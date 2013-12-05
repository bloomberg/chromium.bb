// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_NACL_MODULES_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_NACL_MODULES_HANDLER_H_

#include <list>
#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// An NaCl module included in the extension.
struct NaClModuleInfo {
  typedef std::list<NaClModuleInfo> List;
  // Returns the NaCl modules for the extensions, or NULL if none exist.
  static const NaClModuleInfo::List* GetNaClModules(
      const Extension* extension);

  GURL url;
  std::string mime_type;
};

// Parses the "nacl_modules" manifest key.
class NaClModulesHandler : public ManifestHandler {
 public:
  NaClModulesHandler();
  virtual ~NaClModulesHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_NACL_MODULES_HANDLER_H_
