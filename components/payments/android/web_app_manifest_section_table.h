// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_ANDROID_WEB_APP_MANIFEST_SECTION_TABLE_H_
#define COMPONENTS_PAYMENTS_ANDROID_WEB_APP_MANIFEST_SECTION_TABLE_H_

#include <string>
#include <vector>

#include "components/payments/content/payment_manifest_parser.mojom.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"

namespace payments {

// This class manages web_app_manifest_section table in SQLite database. It
// expects the following schema.
// The interfaces should only be accessed on DB thread.
//
// web_app_manifest_section The table stores the contents in
//                          WebAppManifestSection.
//
//  id                      The package name of the app.
//  min_version             Minimum version number of the app.
//  fingerprints            The result of SHA256(signing certificate bytes) for
//                          each certificate in the app.
//
class WebAppManifestSectionTable : public WebDatabaseTable {
 public:
  WebAppManifestSectionTable();
  ~WebAppManifestSectionTable() override;

  // Retrieves the WebAppManifestSectionTable* owned by |db|.
  static WebAppManifestSectionTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool IsSyncable() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Adds the web app |*manifest|. Note that the previous web app manifest will
  // be deleted.
  bool AddWebAppManifest(mojom::WebAppManifestSection* manifest);

  // Gets manifest of the |web_app|. Returns nullptr if no manifest exists for
  // the |web_app|.
  mojom::WebAppManifestSectionPtr GetWebAppManifest(const std::string& web_app);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppManifestSectionTable);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_ANDROID_WEB_APP_MANIFEST_SECTION_TABLE_H_
