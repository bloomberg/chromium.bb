// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
#pragma once

#include <vector>

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/meta_table.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/keyword_table.h"

class AutofillTableTest;
class FilePath;
class GURL;
class NotificationService;
class SkBitmap;

namespace base {
class Time;
}

namespace webkit_glue {
struct PasswordForm;
}

#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif

////////////////////////////////////////////////////////////////////////////////
//
// A Sqlite database instance to store all the meta data we have about web pages
//
////////////////////////////////////////////////////////////////////////////////
class WebDatabase {
 public:
  WebDatabase();
  virtual ~WebDatabase();

  // Initialize the database given a name. The name defines where the sqlite
  // file is. If this returns an error code, no other method should be called.
  sql::InitStatus Init(const FilePath& db_name);

  // Transactions management
  void BeginTransaction();
  void CommitTransaction();

  virtual AutofillTable* GetAutofillTable();
  virtual KeywordTable* GetKeywordTable();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager support
  //
  //////////////////////////////////////////////////////////////////////////////

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

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps
  //
  //////////////////////////////////////////////////////////////////////////////

  bool SetWebAppImage(const GURL& url, const SkBitmap& image);
  bool GetWebAppImages(const GURL& url, std::vector<SkBitmap>* images);

  bool SetWebAppHasAllImages(const GURL& url, bool has_all_images);
  bool GetWebAppHasAllImages(const GURL& url);

  bool RemoveWebApp(const GURL& url);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Token Service
  //
  //////////////////////////////////////////////////////////////////////////////

  // Remove all tokens previously set with SetTokenForService.
  bool RemoveAllTokens();

  // Retrieves all tokens previously set with SetTokenForService.
  // Returns true if there were tokens and we decrypted them,
  // false if there was a failure somehow
  bool GetAllTokens(std::map<std::string, std::string>* tokens);

  // Store a token in the token_service table. Stored encrypted. May cause
  // a mac keychain popup.
  // True if we encrypted a token and stored it, false otherwise.
  bool SetTokenForService(const std::string& service,
                          const std::string& token);

 private:
  // TODO(andybons): Remove this in the next refactor round.
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_RemoveBetweenChanges);

  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_UpdateDontReplace);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddFormFieldValues);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateAutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrash);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrashInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveAutofillProfilesAndCreditCardsModifiedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, CreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateCreditCard);

  bool InitLoginsTable();
  bool InitTokenServiceTable();
  bool InitWebAppIconsTable();
  bool InitWebAppsTable();

  // Used by |Init()| to migration database schema from older versions to
  // current version.
  sql::InitStatus MigrateOldVersionsAsNeeded();

  sql::Connection db_;
  sql::MetaTable meta_table_;

  scoped_ptr<AutofillTable> autofill_table_;
  scoped_ptr<KeywordTable> keyword_table_;

  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
