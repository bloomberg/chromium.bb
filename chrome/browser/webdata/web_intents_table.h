// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
#define CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "chrome/browser/webdata/web_database_table.h"

// Describes the relevant elements of a WebIntent.
// TODO(groby): Will need to be moved to different place once more
// infrastructure for WebIntents is in place.
struct WebIntentData {
  GURL service_url; // URL for service invocation.
  string16 action; // Name of action provided by service.
  string16 type; // MIME type of data accepted by service.
};

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
  virtual bool Init();
  virtual bool IsSyncable();

  // Adds a web intent to the WebIntents table. If intent already exists,
  // replaces it.
  bool SetWebIntent(const string16& action,
                    const string16& type,
                    const GURL& service_url);

  // Retrieve all intents from WebIntents table that match |action|.
  bool GetWebIntents(const string16& action,
                     std::vector<WebIntentData>* intents);

  // Removes intent from WebIntents table - must match all parameters exactly.
  bool RemoveWebIntent(const string16& action,
                       const string16& type,
                       const GURL& service_url);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIntentsTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_INTENTS_TABLE_H_
