// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
#define CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/webdata/web_database_table.h"

namespace sql {
class Connection;
class MetaTable;
}

struct DefaultWebIntentService;
class WebDatabase;

// TODO(thakis): Delete this class once there's a migration that drops the
// table backing it.

// This class manages the WebIntents table within the SQLite database passed
// to the constructor. It expects the following schema:
//
// web_intents
//    service_url       URL for service invocation.
//    action            Name of action provided by the service.
//    type              MIME type of data accepted by the service.
//    title             Title for the service page
//    disposition       Either 'window' or 'inline' disposition.
//
// Web Intent Services are uniquely identified by the <service_url,action,type>
// tuple.
//
// Also manages the defaults table:
//
// web_intents_defaults
//    action            Intent action for this default.
//    type              Intent type for this default.
//    url_prefix        URL prefix for which the default is invoked.
//    user_date         Epoch time when the user made this default.
//    suppression       Set if the default is (temporarily) suppressed.
//    service_url       The URL of a service in the web_intents table.
//    extension_url     The URL for an extension handling intents.
//
// The defaults are scoped by action, then type, then url prefix.
//
class WebIntentsTable : public WebDatabaseTable {
 public:
  WebIntentsTable();
  virtual ~WebIntentsTable();

  // Retrieves the WebIntentsTable* owned by |database|.
  static WebIntentsTable* FromWebDatabase(WebDatabase* database);

  // WebDatabaseTable implementation.
  virtual WebDatabaseTable::TypeKey GetTypeKey() const OVERRIDE;
  virtual bool Init(sql::Connection* db, sql::MetaTable* meta_table) OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;
  virtual bool MigrateToVersion(int version,
                                const std::string& app_locale,
                                bool* update_compatible_version) OVERRIDE;

  // Adds "scheme" column to the web_intents and web_intents_defaults tables.
  bool MigrateToVersion46AddSchemeColumn();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIntentsTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
