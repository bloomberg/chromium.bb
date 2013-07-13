// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_STORAGE_STORAGE_SCHEMA_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_STORAGE_STORAGE_SCHEMA_MANIFEST_HANDLER_H_

#include "base/basictypes.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace policy {
class PolicySchema;
}

namespace extensions {

// Handles the "storage.managed_schema" manifest key.
class StorageSchemaManifestHandler : public ManifestHandler {
 public:
  StorageSchemaManifestHandler();
  virtual ~StorageSchemaManifestHandler();

  // Returns the managed storage schema defined for |extension|.
  // If the schema is invalid then NULL is returned, and the failure reason
  // is stored in |error|.
  // This function does file I/O and must be called on a thread that allows I/O.
  static scoped_ptr<policy::PolicySchema> GetSchema(const Extension* extension,
                                                    std::string* error);

 private:
  // ManifestHandler implementation:
  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(StorageSchemaManifestHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_STORAGE_STORAGE_SCHEMA_MANIFEST_HANDLER_H_
