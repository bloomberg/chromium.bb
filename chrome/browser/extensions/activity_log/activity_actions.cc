// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"

namespace extensions {

using api::activity_log_private::ExtensionActivity;

const char* Action::kTableBasicFields =
    "extension_id LONGVARCHAR NOT NULL, "
    "time INTEGER NOT NULL";

Action::Action(const std::string& extension_id,
               const base::Time& time,
               ExtensionActivity::ActivityType activity_type)
    : extension_id_(extension_id),
      time_(time),
      activity_type_(activity_type) {}

// static
bool Action::InitializeTableInternal(sql::Connection* db,
                                     const char* table_name,
                                     const char* content_fields[],
                                     const char* field_types[],
                                     const int num_content_fields) {
  if (!db->DoesTableExist(table_name)) {
    std::string table_creator = base::StringPrintf(
        "CREATE TABLE %s (%s", table_name, kTableBasicFields);
    for (int i = 0; i < num_content_fields; i++) {
      table_creator += base::StringPrintf(", %s %s",
                                          content_fields[i],
                                          field_types[i]);
    }
    table_creator += ")";
    if (!db->Execute(table_creator.c_str()))
      return false;
  } else {
    // In case we ever want to add new fields, this initializes them to be
    // empty strings.
    for (int i = 0; i < num_content_fields; i++) {
      if (!db->DoesColumnExist(table_name, content_fields[i])) {
        std::string table_updater = base::StringPrintf(
            "ALTER TABLE %s ADD COLUMN %s %s; ",
             table_name,
             content_fields[i],
             field_types[i]);
        if (!db->Execute(table_updater.c_str()))
          return false;
      }
    }
  }
  return true;
}

}  // namespace extensions

