// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_LOGINS_TABLE_H_
#define CHROME_BROWSER_WEBDATA_LOGINS_TABLE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/webdata/web_database_table.h"

namespace base {
class Time;
}

namespace webkit_glue {
struct PasswordForm;
}

#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif

// This class manages the logins table within the SQLite database passed to the
// constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
//
// logins
//   origin_url
//   action_url
//   username_element
//   username_value
//   password_element
//   password_value
//   submit_element
//   signon_realm        The authority (scheme, host, port).
//   ssl_valid           SSL status of page containing the form at first
//                       impression.
//   preferred           MRU bit.
//   date_created        This column was added after logins support. "Legacy"
//                       entries have a value of 0.
//   blacklisted_by_user Tracks whether or not the user opted to 'never
//                       remember'
//                       passwords for this site.
//
class LoginsTable : public WebDatabaseTable {
 public:
  LoginsTable(sql::Connection* db, sql::MetaTable* meta_table)
      : WebDatabaseTable(db, meta_table) {}
  virtual ~LoginsTable() {}
  virtual bool Init() OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;

  // Adds |form| to the list of remembered password forms.
  bool AddLogin(const webkit_glue::PasswordForm& form);

#if defined(OS_WIN)
  // Adds |info| to the list of imported passwords from ie7/ie8.
  bool AddIE7Login(const IE7PasswordInfo& info);

  // Removes |info| from the list of imported passwords from ie7/ie8.
  bool RemoveIE7Login(const IE7PasswordInfo& info);

  // Return the ie7/ie8 login matching |info|.
  bool GetIE7Login(const IE7PasswordInfo& info, IE7PasswordInfo* result);
#endif

  // Updates remembered password form.
  bool UpdateLogin(const webkit_glue::PasswordForm& form);

  // Removes |form| from the list of remembered password forms.
  bool RemoveLogin(const webkit_glue::PasswordForm& form);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end);

  // Loads a list of matching password forms into the specified vector |forms|.
  // The list will contain all possibly relevant entries to the observed |form|,
  // including blacklisted matches.
  bool GetLogins(const webkit_glue::PasswordForm& form,
                 std::vector<webkit_glue::PasswordForm*>* forms);

  // Loads the complete list of password forms into the specified vector |forms|
  // if include_blacklisted is true, otherwise only loads those which are
  // actually autofill-able; i.e haven't been blacklisted by the user selecting
  // the 'Never for this site' button.
  bool GetAllLogins(std::vector<webkit_glue::PasswordForm*>* forms,
                    bool include_blacklisted);

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginsTable);
};

#endif  // CHROME_BROWSER_WEBDATA_LOGINS_TABLE_H_
