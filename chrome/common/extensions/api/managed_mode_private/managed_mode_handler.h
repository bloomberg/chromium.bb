// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_MANAGED_MODE_PRIVATE_MANAGED_MODE_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_MANAGED_MODE_PRIVATE_MANAGED_MODE_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "extensions/common/extension_resource.h"

namespace extensions {

struct ManagedModeInfo : public Extension::ManifestData {
  ManagedModeInfo();
  virtual ~ManagedModeInfo();

  static bool IsContentPack(const Extension* extension);
  static ExtensionResource GetContentPackSiteList(const Extension* extension);

  // A file containing a list of sites for Managed Mode.
  base::FilePath site_list;
};

// Parses the "content_pack" manifest key for Managed Mode.
class ManagedModeHandler : public ManifestHandler {
 public:
  ManagedModeHandler();
  virtual ~ManagedModeHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  bool LoadSites(ManagedModeInfo* info,
                 const base::DictionaryValue* content_pack_value,
                 string16* error);
  bool LoadConfigurations(ManagedModeInfo* info,
                          const base::DictionaryValue* content_pack_value,
                          string16* error);

  DISALLOW_COPY_AND_ASSIGN(ManagedModeHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_MANAGED_MODE_PRIVATE_MANAGED_MODE_HANDLER_H_
