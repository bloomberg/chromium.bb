// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SUPERVISED_USER_PRIVATE_SUPERVISED_USER_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SUPERVISED_USER_PRIVATE_SUPERVISED_USER_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct SupervisedUserInfo : public Extension::ManifestData {
  SupervisedUserInfo();
  virtual ~SupervisedUserInfo();

  static bool IsContentPack(const Extension* extension);
  static ExtensionResource GetContentPackSiteList(const Extension* extension);

  // A file containing a list of sites for a supervised user.
  base::FilePath site_list;
};

// Parses the "content_pack" manifest key for a supervised user.
class SupervisedUserHandler : public ManifestHandler {
 public:
  SupervisedUserHandler();
  virtual ~SupervisedUserHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  bool LoadSites(SupervisedUserInfo* info,
                 const base::DictionaryValue* content_pack_value,
                 base::string16* error);
  bool LoadConfigurations(SupervisedUserInfo* info,
                          const base::DictionaryValue* content_pack_value,
                          base::string16* error);

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SUPERVISED_USER_PRIVATE_SUPERVISED_USER_HANDLER_H_
