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
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_id.h"

class AutofillChange;
class AutofillEntry;
class AutoFillProfile;
class CreditCard;
class FilePath;
class GURL;
class NotificationService;
class SkBitmap;
class TemplateURL;
class WebDatabaseTest;

namespace base {
class Time;
}

namespace webkit_glue {
class FormField;
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

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds a new keyword, updating the id field on success.
  // Returns true if successful.
  bool AddKeyword(const TemplateURL& url);

  // Removes the specified keyword.
  // Returns true if successful.
  bool RemoveKeyword(TemplateURLID id);

  // Loads the keywords into the specified vector. It's up to the caller to
  // delete the returned objects.
  // Returns true on success.
  bool GetKeywords(std::vector<TemplateURL*>* urls);

  // Updates the database values for the specified url.
  // Returns true on success.
  bool UpdateKeyword(const TemplateURL& url);

  // ID (TemplateURL->id) of the default search provider.
  bool SetDefaultSearchProviderID(int64 id);
  int64 GetDefaulSearchProviderID();

  // Version of the builtin keywords.
  bool SetBuitinKeywordVersion(int version);
  int GetBuitinKeywordVersion();

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
  // AutoFill
  //
  //////////////////////////////////////////////////////////////////////////////

  // Records the form elements in |elements| in the database in the
  // autofill table.  A list of all added and updated autofill entries
  // is returned in the changes out parameter.
  bool AddFormFieldValues(const std::vector<webkit_glue::FormField>& elements,
                          std::vector<AutofillChange>* changes);

  // Records a single form element in the database in the autofill table. A list
  // of all added and updated autofill entries is returned in the changes out
  // parameter.
  bool AddFormFieldValue(const webkit_glue::FormField& element,
                         std::vector<AutofillChange>* changes);

  // Retrieves a vector of all values which have been recorded in the autofill
  // table as the value in a form element with name |name| and which start with
  // |prefix|.  The comparison of the prefix is case insensitive.
  bool GetFormValuesForElementName(const string16& name,
                                   const string16& prefix,
                                   std::vector<string16>* values,
                                   int limit);

  // Removes rows from autofill_dates if they were created on or after
  // |delete_begin| and strictly before |delete_end|.  Decrements the
  // count of the corresponding rows in the autofill table, and
  // removes those rows if the count goes to 0.  A list of all changed
  // keys and whether each was updater or removed is returned in the
  // changes out parameter.
  bool RemoveFormElementsAddedBetween(base::Time delete_begin,
                                      base::Time delete_end,
                                      std::vector<AutofillChange>* changes);

  // Removes from autofill_dates rows with given pair_id where date_created lies
  // between delte_begin and delte_end.
  bool RemoveFormElementForTimeRange(int64 pair_id,
                                     base::Time delete_begin,
                                     base::Time delete_end,
                                     int* how_many);

  // Increments the count in the row corresponding to |pair_id| by
  // |delta|.  Removes the row from the table and sets the
  // |was_removed| out parameter to true if the count becomes 0.
  bool AddToCountOfFormElement(int64 pair_id, int delta, bool* was_removed);

  // Gets the pair_id and count entries from name and value specified in
  // |element|.  Sets *pair_id and *count to 0 if there is no such row in
  // the table.
  bool GetIDAndCountOfFormElement(const webkit_glue::FormField& element,
                                  int64* pair_id,
                                  int* count);

  // Gets the count only given the pair_id.
  bool GetCountOfFormElement(int64 pair_id, int* count);

  // Updates the count entry in the row corresponding to |pair_id| to |count|.
  bool SetCountOfFormElement(int64 pair_id, int count);

  // Adds a new row to the autofill table with name and value given in
  // |element|.  Sets *pair_id to the pair_id of the new row.
  bool InsertFormElement(const webkit_glue::FormField& element, int64* pair_id);

  // Adds a new row to the autofill_dates table.
  bool InsertPairIDAndDate(int64 pair_id, base::Time date_created);

  // Removes row from the autofill tables given |pair_id|.
  bool RemoveFormElementForID(int64 pair_id);

  // Removes row from the autofill tables for the given |name| |value| pair.
  virtual bool RemoveFormElement(const string16& name, const string16& value);

  // Retrieves all of the entries in the autofill table.
  virtual bool GetAllAutofillEntries(std::vector<AutofillEntry>* entries);

  // Retrieves a single entry from the autofill table.
  virtual bool GetAutofillTimestamps(const string16& name,
                             const string16& value,
                             std::vector<base::Time>* timestamps);

  // Replaces existing autofill entries with the entries supplied in
  // the argument.  If the entry does not already exist, it will be
  // added.
  virtual bool UpdateAutofillEntries(const std::vector<AutofillEntry>& entries);

  // Records a single AutoFill profile in the autofill_profiles table.
  virtual bool AddAutoFillProfile(const AutoFillProfile& profile);

  // Updates the database values for the specified profile.
  virtual bool UpdateAutoFillProfile(const AutoFillProfile& profile);

  // Removes a row from the autofill_profiles table.  |guid| is the identifier
  // of the profile to remove.
  virtual bool RemoveAutoFillProfile(const std::string& guid);

  // Retrieves a profile with guid |guid|.  The caller owns |profile|.
  bool GetAutoFillProfile(const std::string& guid, AutoFillProfile** profile);

  // Retrieves all profiles in the database.  Caller owns the returned profiles.
  virtual bool GetAutoFillProfiles(std::vector<AutoFillProfile*>* profiles);

  // Records a single credit card in the credit_cards table.
  bool AddCreditCard(const CreditCard& credit_card);

  // Updates the database values for the specified credit card.
  bool UpdateCreditCard(const CreditCard& credit_card);

  // Removes a row from the credit_cards table.  |guid| is the identifer  of the
  // credit card to remove.
  bool RemoveCreditCard(const std::string& guid);

  // Retrieves a credit card with guid |guid|.  The caller owns
  // |credit_card_id|.
  bool GetCreditCard(const std::string& guid, CreditCard** credit_card);

  // Retrieves all credit cards in the database.  Caller owns the returned
  // credit cards.
  virtual bool GetCreditCards(std::vector<CreditCard*>* credit_cards);

  // Removes rows from autofill_profiles and credit_cards if they were created
  // on or after |delete_begin| and strictly before |delete_end|.
  bool RemoveAutoFillProfilesAndCreditCardsModifiedBetween(
      base::Time delete_begin,
      base::Time delete_end);

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
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, Autofill);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, Autofill_AddChanges);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, Autofill_RemoveBetweenChanges);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest,
                           Autofill_GetAllAutofillEntries_OneResult);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest,
                           Autofill_GetAllAutofillEntries_TwoDistinct);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest,
                           Autofill_GetAllAutofillEntries_TwoSame);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, Autofill_UpdateDontReplace);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, Autofill_AddFormFieldValues);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, AutoFillProfile);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, CreditCard);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, UpdateAutoFillProfile);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest, UpdateCreditCard);
  FRIEND_TEST_ALL_PREFIXES(WebDatabaseTest,
                           RemoveAutoFillProfilesAndCreditCardsModifiedBetween);

  // Methods for adding autofill entries at a specified time.  For
  // testing only.
  bool AddFormFieldValuesTime(
      const std::vector<webkit_glue::FormField>& elements,
      std::vector<AutofillChange>* changes,
      base::Time time);
  bool AddFormFieldValueTime(const webkit_glue::FormField& element,
                             std::vector<AutofillChange>* changes,
                             base::Time time);

  // Removes empty values for autofill that were incorrectly stored in the DB
  // (see bug http://crbug.com/6111).
  // TODO(jcampan): http://crbug.com/7564 remove when we think all users have
  //                run this code.
  bool ClearAutofillEmptyValueElements();

  // Insert a single AutofillEntry into the autofill/autofill_dates tables.
  bool InsertAutofillEntry(const AutofillEntry& entry);

  bool InitKeywordsTable();
  bool InitLoginsTable();
  bool InitAutofillTable();
  bool InitAutofillDatesTable();
  bool InitAutoFillProfilesTable();
  bool InitCreditCardsTable();
  bool InitTokenServiceTable();
  bool InitWebAppIconsTable();
  bool InitWebAppsTable();

  // Used by |Init()| to migration database schema from older versions to
  // current version.
  sql::InitStatus MigrateOldVersionsAsNeeded();

  sql::Connection db_;
  sql::MetaTable meta_table_;

  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H_
