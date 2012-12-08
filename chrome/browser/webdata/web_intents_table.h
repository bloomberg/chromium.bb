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
#include "webkit/glue/web_intent_service_data.h"

namespace sql {
class Connection;
class MetaTable;
}

struct DefaultWebIntentService;

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
  WebIntentsTable(sql::Connection* db, sql::MetaTable* meta_table);
  virtual ~WebIntentsTable();

  // WebDatabaseTable implementation.
  virtual bool Init() OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;

  // Adds "scheme" column to the web_intents and web_intents_defaults tables.
  bool MigrateToVersion46AddSchemeColumn();

  // Adds a web intent service to the WebIntents table.
  // If |service| already exists, replaces it.
  bool SetWebIntentService(const webkit_glue::WebIntentServiceData& service);

  // Retrieve all |services| from WebIntents table that match |action|.
  bool GetWebIntentServicesForAction(
      const string16& action,
      std::vector<webkit_glue::WebIntentServiceData>* services);

  // Retrieve all |services| from WebIntents table that match |scheme|.
  bool GetWebIntentServicesForScheme(
      const string16& scheme,
      std::vector<webkit_glue::WebIntentServiceData>* services);

  // Retrieves all |services| from WebIntents table that match |service_url|.
  bool GetWebIntentServicesForURL(
      const string16& service_url,
      std::vector<webkit_glue::WebIntentServiceData>* services);

  // Retrieve all |services| from WebIntents table.
  bool GetAllWebIntentServices(
      std::vector<webkit_glue::WebIntentServiceData>* services);

  // Removes |service| from WebIntents table - must match all parameters
  // exactly.
  bool RemoveWebIntentService(const webkit_glue::WebIntentServiceData& service);

  // Get the default service to be used for the given intent invocation.
  // If any overlapping defaults are found, they're placed in
  // |default_services|, otherwise, it is untouched.
  // Returns true if the method runs successfully, false on database error.
  bool GetDefaultServices(
      const string16& action,
      std::vector<DefaultWebIntentService>* default_services);

  // Get a list of all installed default services.
  bool GetAllDefaultServices(
      std::vector<DefaultWebIntentService>* default_services);

  // Set a default service to be used on given intent invocations.
  bool SetDefaultService(const DefaultWebIntentService& default_service);

  // Removes a default |service| from table - must match the action, type,
  // and url_pattern parameters exactly.
  bool RemoveDefaultService(const DefaultWebIntentService& default_service);

  // Removes all default services associated with |service_url|.
  bool RemoveServiceDefaults(const GURL& service_url);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIntentsTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
