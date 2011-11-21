// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
#define CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
#pragma once

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

// This class manages the WebIntents table within the SQLite database passed
// to the constructor. It expects the following schema:
//
// web_intents
//    service_url       URL for service invocation.
//    action            Name of action provided by the service.
//    type              MIME type of data accepted by the service.
//
// Intents are uniquely identified by the <service_url,action,type> tuple.
class WebIntentsTable : public WebDatabaseTable {
 public:
  WebIntentsTable(sql::Connection* db, sql::MetaTable* meta_table);
  virtual ~WebIntentsTable();

  // WebDatabaseTable implementation.
  virtual bool Init() OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;

  // Adds a web intent service to the WebIntents table.
  // If |service| already exists, replaces it.
  bool SetWebIntentService(const webkit_glue::WebIntentServiceData& service);

  // Retrieve all |services| from WebIntents table that match |action|.
  bool GetWebIntentServices(
      const string16& action,
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

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIntentsTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
