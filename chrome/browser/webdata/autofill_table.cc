// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_table.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/forms/form_field.h"

using base::Time;
using webkit::forms::FormField;

namespace {

typedef std::vector<Tuple3<int64, string16, string16> > AutofillElementList;

// TODO(dhollowa): Find a common place for this.  It is duplicated in
// personal_data_manager.cc.
template<typename T>
T* address_of(T& v) {
  return &v;
}

string16 LimitDataSize(const string16& data) {
  if (data.size() > AutofillTable::kMaxDataLength)
    return data.substr(0, AutofillTable::kMaxDataLength);

  return data;
}

void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    sql::Statement* s) {
  DCHECK(base::IsValidGUID(profile.guid()));
  s->BindString(0, profile.guid());

  string16 text = profile.GetInfo(COMPANY_NAME);
  s->BindString16(1, LimitDataSize(text));
  text = profile.GetInfo(ADDRESS_HOME_LINE1);
  s->BindString16(2, LimitDataSize(text));
  text = profile.GetInfo(ADDRESS_HOME_LINE2);
  s->BindString16(3, LimitDataSize(text));
  text = profile.GetInfo(ADDRESS_HOME_CITY);
  s->BindString16(4, LimitDataSize(text));
  text = profile.GetInfo(ADDRESS_HOME_STATE);
  s->BindString16(5, LimitDataSize(text));
  text = profile.GetInfo(ADDRESS_HOME_ZIP);
  s->BindString16(6, LimitDataSize(text));
  text = profile.GetInfo(ADDRESS_HOME_COUNTRY);
  s->BindString16(7, LimitDataSize(text));
  std::string country_code = profile.CountryCode();
  s->BindString(8, country_code);
  s->BindInt64(9, Time::Now().ToTimeT());
}

AutofillProfile* AutofillProfileFromStatement(const sql::Statement& s) {
  AutofillProfile* profile = new AutofillProfile;
  profile->set_guid(s.ColumnString(0));
  DCHECK(base::IsValidGUID(profile->guid()));

  profile->SetInfo(COMPANY_NAME, s.ColumnString16(1));
  profile->SetInfo(ADDRESS_HOME_LINE1, s.ColumnString16(2));
  profile->SetInfo(ADDRESS_HOME_LINE2, s.ColumnString16(3));
  profile->SetInfo(ADDRESS_HOME_CITY, s.ColumnString16(4));
  profile->SetInfo(ADDRESS_HOME_STATE, s.ColumnString16(5));
  profile->SetInfo(ADDRESS_HOME_ZIP, s.ColumnString16(6));
  // Intentionally skip column 7, which stores the localized country name.
  profile->SetCountryCode(s.ColumnString(8));
  // Intentionally skip column 9, which stores the profile's modification date.

  return profile;
}

void BindCreditCardToStatement(const CreditCard& credit_card,
                               sql::Statement* s) {
  DCHECK(base::IsValidGUID(credit_card.guid()));
  s->BindString(0, credit_card.guid());

  string16 text = credit_card.GetInfo(CREDIT_CARD_NAME);
  s->BindString16(1, LimitDataSize(text));
  text = credit_card.GetInfo(CREDIT_CARD_EXP_MONTH);
  s->BindString16(2, LimitDataSize(text));
  text = credit_card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  s->BindString16(3, LimitDataSize(text));
  text = credit_card.GetInfo(CREDIT_CARD_NUMBER);
  std::string encrypted_data;
  Encryptor::EncryptString16(text, &encrypted_data);
  s->BindBlob(4, encrypted_data.data(),
              static_cast<int>(encrypted_data.length()));
  s->BindInt64(5, Time::Now().ToTimeT());
}

CreditCard* CreditCardFromStatement(const sql::Statement& s) {
  CreditCard* credit_card = new CreditCard;

  credit_card->set_guid(s.ColumnString(0));
  DCHECK(base::IsValidGUID(credit_card->guid()));

  credit_card->SetInfo(CREDIT_CARD_NAME, s.ColumnString16(1));
  credit_card->SetInfo(CREDIT_CARD_EXP_MONTH, s.ColumnString16(2));
  credit_card->SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, s.ColumnString16(3));
  int encrypted_number_len = s.ColumnByteLength(4);
  string16 credit_card_number;
  if (encrypted_number_len) {
    std::string encrypted_number;
    encrypted_number.resize(encrypted_number_len);
    memcpy(&encrypted_number[0], s.ColumnBlob(4), encrypted_number_len);
    Encryptor::DecryptString16(encrypted_number, &credit_card_number);
  }
  credit_card->SetInfo(CREDIT_CARD_NUMBER, credit_card_number);
  // Intentionally skip column 5, which stores the modification date.

  return credit_card;
}

bool AddAutofillProfileNamesToProfile(sql::Connection* db,
                                      AutofillProfile* profile) {
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, first_name, middle_name, last_name "
      "FROM autofill_profile_names "
      "WHERE guid=?"));
  s.BindString(0, profile->guid());

  if (!s.is_valid())
    return false;

  std::vector<string16> first_names;
  std::vector<string16> middle_names;
  std::vector<string16> last_names;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    first_names.push_back(s.ColumnString16(1));
    middle_names.push_back(s.ColumnString16(2));
    last_names.push_back(s.ColumnString16(3));
  }
  if (!s.Succeeded())
    return false;

  profile->SetMultiInfo(NAME_FIRST, first_names);
  profile->SetMultiInfo(NAME_MIDDLE, middle_names);
  profile->SetMultiInfo(NAME_LAST, last_names);
  return true;
}

bool AddAutofillProfileEmailsToProfile(sql::Connection* db,
                                       AutofillProfile* profile) {
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, email "
      "FROM autofill_profile_emails "
      "WHERE guid=?"));
  s.BindString(0, profile->guid());

  if (!s.is_valid())
    return false;

  std::vector<string16> emails;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    emails.push_back(s.ColumnString16(1));
  }
  if (!s.Succeeded())
    return false;

  profile->SetMultiInfo(EMAIL_ADDRESS, emails);
  return true;
}

bool AddAutofillProfilePhonesToProfile(sql::Connection* db,
                                       AutofillProfile* profile) {
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, type, number "
      "FROM autofill_profile_phones "
      "WHERE guid=? AND type=?"));

  // Value used to be either [(0, phone), (1, fax)] but fax has been removed.
  s.BindString(0, profile->guid());
  s.BindInt(1, 0);

  if (!s.is_valid())
    return false;

  std::vector<string16> numbers;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    numbers.push_back(s.ColumnString16(2));
  }
  if (!s.Succeeded())
    return false;

  profile->SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, numbers);
  return true;
}

bool AddAutofillProfileNames(const AutofillProfile& profile,
                             sql::Connection* db) {
  std::vector<string16> first_names;
  profile.GetMultiInfo(NAME_FIRST, &first_names);
  std::vector<string16> middle_names;
  profile.GetMultiInfo(NAME_MIDDLE, &middle_names);
  std::vector<string16> last_names;
  profile.GetMultiInfo(NAME_LAST, &last_names);
  DCHECK_EQ(first_names.size(), middle_names.size());
  DCHECK_EQ(middle_names.size(), last_names.size());

  for (size_t i = 0; i < first_names.size(); ++i) {
    // Add the new name.
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_names"
      " (guid, first_name, middle_name, last_name) "
      "VALUES (?,?,?,?)"));
    s.BindString(0, profile.guid());
    s.BindString16(1, first_names[i]);
    s.BindString16(2, middle_names[i]);
    s.BindString16(3, last_names[i]);

    if (!s.Run())
      return false;
  }
  return true;
}

bool AddAutofillProfileEmails(const AutofillProfile& profile,
                              sql::Connection* db) {
  std::vector<string16> emails;
  profile.GetMultiInfo(EMAIL_ADDRESS, &emails);

  for (size_t i = 0; i < emails.size(); ++i) {
    // Add the new email.
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_emails"
      " (guid, email) "
      "VALUES (?,?)"));
    s.BindString(0, profile.guid());
    s.BindString16(1, emails[i]);

    if (!s.Run())
      return false;
  }

  return true;
}

bool AddAutofillProfilePhones(const AutofillProfile& profile,
                              sql::Connection* db) {
  std::vector<string16> numbers;
  profile.GetMultiInfo(PHONE_HOME_WHOLE_NUMBER, &numbers);

  for (size_t i = 0; i < numbers.size(); ++i) {
    // Add the new number.
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_phones"
      " (guid, type, number) "
      "VALUES (?,?,?)"));
    s.BindString(0, profile.guid());
    // Value used to be either [(0, phone), (1, fax)] but fax has been removed.
    s.BindInt(1, 0);
    s.BindString16(2, numbers[i]);

    if (!s.Run())
      return false;
  }

  return true;
}

bool AddAutofillProfilePieces(const AutofillProfile& profile,
                              sql::Connection* db) {
  if (!AddAutofillProfileNames(profile, db))
    return false;

  if (!AddAutofillProfileEmails(profile, db))
    return false;

  if (!AddAutofillProfilePhones(profile, db))
    return false;

  return true;
}

bool RemoveAutofillProfilePieces(const std::string& guid, sql::Connection* db) {
  sql::Statement s1(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_names WHERE guid = ?"));
  s1.BindString(0, guid);

  if (!s1.Run())
    return false;

  sql::Statement s2(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_emails WHERE guid = ?"));
  s2.BindString(0, guid);

  if (!s2.Run())
    return false;

  sql::Statement s3(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_phones WHERE guid = ?"));
  s3.BindString(0, guid);

  return s3.Run();
}

}  // namespace

// The maximum length allowed for form data.
const size_t AutofillTable::kMaxDataLength = 1024;

AutofillTable::AutofillTable(sql::Connection* db, sql::MetaTable* meta_table)
    : WebDatabaseTable(db, meta_table) {
}

AutofillTable::~AutofillTable() {
}

bool AutofillTable::Init() {
 return (InitMainTable() && InitCreditCardsTable() && InitDatesTable() &&
     InitProfilesTable() && InitProfileNamesTable() &&
     InitProfileEmailsTable() && InitProfilePhonesTable() &&
     InitProfileTrashTable());
}

bool AutofillTable::IsSyncable() {
  return true;
}

bool AutofillTable::AddFormFieldValues(const std::vector<FormField>& elements,
                                       std::vector<AutofillChange>* changes) {
  return AddFormFieldValuesTime(elements, changes, Time::Now());
}

bool AutofillTable::AddFormFieldValue(const FormField& element,
                                      std::vector<AutofillChange>* changes) {
  return AddFormFieldValueTime(element, changes, Time::Now());
}

bool AutofillTable::GetFormValuesForElementName(const string16& name,
                                                const string16& prefix,
                                                std::vector<string16>* values,
                                                int limit) {
  DCHECK(values);
  sql::Statement s;

  if (prefix.empty()) {
    s.Assign(db_->GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    s.BindString16(0, name);
    s.BindInt(1, limit);
  } else {
    string16 prefix_lower = base::i18n::ToLower(prefix);
    string16 next_prefix = prefix_lower;
    next_prefix[next_prefix.length() - 1]++;

    s.Assign(db_->GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? AND "
        "value_lower >= ? AND "
        "value_lower < ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    s.BindString16(0, name);
    s.BindString16(1, prefix_lower);
    s.BindString16(2, next_prefix);
    s.BindInt(3, limit);
  }

  values->clear();
  while (s.Step())
    values->push_back(s.ColumnString16(0));
  return s.Succeeded();
}

bool AutofillTable::RemoveFormElementsAddedBetween(
    const Time& delete_begin,
    const Time& delete_end,
    std::vector<AutofillChange>* changes) {
  DCHECK(changes);
  // Query for the pair_id, name, and value of all form elements that
  // were used between the given times.
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT DISTINCT a.pair_id, a.name, a.value "
      "FROM autofill_dates ad JOIN autofill a ON ad.pair_id = a.pair_id "
      "WHERE ad.date_created >= ? AND ad.date_created < ?"));
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1,
              (delete_end.is_null() || delete_end == base::Time::Max()) ?
                  std::numeric_limits<int64>::max() :
                  delete_end.ToTimeT());

  AutofillElementList elements;
  while (s.Step()) {
    elements.push_back(MakeTuple(s.ColumnInt64(0),
                                 s.ColumnString16(1),
                                 s.ColumnString16(2)));
  }
  if (!s.Succeeded())
    return false;

  for (AutofillElementList::iterator itr = elements.begin();
       itr != elements.end(); ++itr) {
    int how_many = 0;
    if (!RemoveFormElementForTimeRange(itr->a, delete_begin, delete_end,
                                       &how_many)) {
      return false;
    }
    // We store at most 2 time stamps. If we remove both of them we should
    // delete the corresponding data. If we delete only one it could still be
    // the last timestamp for the data, so check how many timestamps do remain.
    bool should_remove = (CountTimestampsData(itr->a) == 0);
    if (should_remove) {
      if (!RemoveFormElementForID(itr->a))
        return false;
    } else {
      if (!AddToCountOfFormElement(itr->a, -how_many))
        return false;
    }
    AutofillChange::Type change_type =
        should_remove ? AutofillChange::REMOVE : AutofillChange::UPDATE;
    changes->push_back(AutofillChange(change_type,
                                      AutofillKey(itr->b, itr->c)));
  }

  return true;
}

bool AutofillTable::RemoveExpiredFormElements(
    std::vector<AutofillChange>* changes) {
  DCHECK(changes);

  base::Time delete_end = AutofillEntry::ExpirationTime();
  // Query for the pair_id, name, and value of all form elements that
  // were last used before the |delete_end|.
  sql::Statement select_for_delete(db_->GetUniqueStatement(
      "SELECT DISTINCT pair_id, name, value "
      "FROM autofill WHERE pair_id NOT IN "
      "(SELECT DISTINCT pair_id "
      "FROM autofill_dates WHERE date_created >= ?)"));
  select_for_delete.BindInt64(0, delete_end.ToTimeT());
  AutofillElementList entries_to_delete;
  while (select_for_delete.Step()) {
    entries_to_delete.push_back(MakeTuple(select_for_delete.ColumnInt64(0),
                                          select_for_delete.ColumnString16(1),
                                          select_for_delete.ColumnString16(2)));
  }

  if (!select_for_delete.Succeeded())
    return false;

  sql::Statement delete_data_statement(db_->GetUniqueStatement(
      "DELETE FROM autofill WHERE pair_id NOT IN ("
      "SELECT pair_id FROM autofill_dates WHERE date_created >= ?)"));
  delete_data_statement.BindInt64(0, delete_end.ToTimeT());
  if (!delete_data_statement.Run())
    return false;

  sql::Statement delete_times_statement(db_->GetUniqueStatement(
      "DELETE FROM autofill_dates WHERE pair_id NOT IN ("
      "SELECT pair_id FROM autofill_dates WHERE date_created >= ?)"));
  delete_times_statement.BindInt64(0, delete_end.ToTimeT());
  if (!delete_times_statement.Run())
    return false;

  // Cull remaining entries' timestamps.
  std::vector<AutofillEntry> entries;
  if (!GetAllAutofillEntries(&entries))
    return false;
  sql::Statement cull_date_entry(db_->GetUniqueStatement(
      "DELETE FROM autofill_dates "
      "WHERE pair_id == (SELECT pair_id FROM autofill "
                         "WHERE name = ? and value = ?)"
      "AND date_created != ? AND date_created != ?"));
  for (size_t i = 0; i < entries.size(); ++i) {
    cull_date_entry.BindString16(0, entries[i].key().name());
    cull_date_entry.BindString16(1, entries[i].key().value());
    cull_date_entry.BindInt64(2,
        entries[i].timestamps().empty() ? 0 :
        entries[i].timestamps().front().ToTimeT());
    cull_date_entry.BindInt64(3,
        entries[i].timestamps().empty() ? 0 :
        entries[i].timestamps().back().ToTimeT());
    if (!cull_date_entry.Run())
      return false;
    cull_date_entry.Reset(true);
  }

  changes->clear();
  changes->reserve(entries_to_delete.size());

  for (AutofillElementList::iterator it = entries_to_delete.begin();
       it != entries_to_delete.end(); ++it) {
    changes->push_back(AutofillChange(
        AutofillChange::REMOVE, AutofillKey(it->b, it->c)));
  }
  return true;
}

bool AutofillTable::RemoveFormElementForTimeRange(int64 pair_id,
                                                  const Time& delete_begin,
                                                  const Time& delete_end,
                                                  int* how_many) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_dates WHERE pair_id = ? AND "
      "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, pair_id);
  s.BindInt64(1, delete_begin.is_null() ? 0 : delete_begin.ToTimeT());
  s.BindInt64(2, delete_end.is_null() ? std::numeric_limits<int64>::max() :
                                        delete_end.ToTimeT());

  bool result = s.Run();
  if (how_many)
    *how_many = db_->GetLastChangeCount();

  return result;
}

int AutofillTable::CountTimestampsData(int64 pair_id) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT COUNT(*) FROM autofill_dates WHERE pair_id = ?"));
  s.BindInt64(0, pair_id);
  if (!s.Step()) {
    NOTREACHED();
    return 0;
  } else {
    return s.ColumnInt(0);
  }
}

bool AutofillTable::AddToCountOfFormElement(int64 pair_id,
                                            int delta) {
  int count = 0;

  if (!GetCountOfFormElement(pair_id, &count))
    return false;

  if (count + delta == 0) {
    // Should remove the element earlier in the code.
    NOTREACHED();
    return false;
  } else {
    if (!SetCountOfFormElement(pair_id, count + delta))
      return false;
  }
  return true;
}

bool AutofillTable::GetIDAndCountOfFormElement(
    const FormField& element,
    int64* pair_id,
    int* count) {
  DCHECK(pair_id);
  DCHECK(count);

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT pair_id, count FROM autofill "
      "WHERE name = ? AND value = ?"));
  s.BindString16(0, element.name);
  s.BindString16(1, element.value);

  if (!s.is_valid())
    return false;

  *pair_id = 0;
  *count = 0;

  if (s.Step()) {
    *pair_id = s.ColumnInt64(0);
    *count = s.ColumnInt(1);
  }

  return true;
}

bool AutofillTable::GetCountOfFormElement(int64 pair_id, int* count) {
  DCHECK(count);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT count FROM autofill WHERE pair_id = ?"));
  s.BindInt64(0, pair_id);

  if (s.Step()) {
    *count = s.ColumnInt(0);
    return true;
  }
  return false;
}

bool AutofillTable::SetCountOfFormElement(int64 pair_id, int count) {
  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE autofill SET count = ? WHERE pair_id = ?"));
  s.BindInt(0, count);
  s.BindInt64(1, pair_id);

  return s.Run();
}

bool AutofillTable::InsertFormElement(const FormField& element,
                                      int64* pair_id) {
  DCHECK(pair_id);
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO autofill (name, value, value_lower) VALUES (?,?,?)"));
  s.BindString16(0, element.name);
  s.BindString16(1, element.value);
  s.BindString16(2, base::i18n::ToLower(element.value));

  if (!s.Run())
    return false;

  *pair_id = db_->GetLastInsertRowId();
  return true;
}

bool AutofillTable::InsertPairIDAndDate(int64 pair_id,
                                        const Time& date_created) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO autofill_dates "
      "(pair_id, date_created) VALUES (?, ?)"));
  s.BindInt64(0, pair_id);
  s.BindInt64(1, date_created.ToTimeT());

  return s.Run();
}

bool AutofillTable::DeleteLastAccess(int64 pair_id) {
  // Inner SELECT selects the newest |date_created| for a given |pair_id|.
  // DELETE deletes only that entry.
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_dates WHERE pair_id = ? and date_created IN "
      "(SELECT date_created FROM autofill_dates WHERE pair_id = ? "
      "ORDER BY date_created DESC LIMIT 1)"));
  s.BindInt64(0, pair_id);
  s.BindInt64(1, pair_id);

  return s.Run();
}

bool AutofillTable::AddFormFieldValuesTime(
    const std::vector<FormField>& elements,
    std::vector<AutofillChange>* changes,
    Time time) {
  // Only add one new entry for each unique element name.  Use |seen_names| to
  // track this.  Add up to |kMaximumUniqueNames| unique entries per form.
  const size_t kMaximumUniqueNames = 256;
  std::set<string16> seen_names;
  bool result = true;
  for (std::vector<FormField>::const_iterator itr = elements.begin();
       itr != elements.end(); ++itr) {
    if (seen_names.size() >= kMaximumUniqueNames)
      break;
    if (seen_names.find(itr->name) != seen_names.end())
      continue;
    result = result && AddFormFieldValueTime(*itr, changes, time);
    seen_names.insert(itr->name);
  }
  return result;
}

bool AutofillTable::ClearAutofillEmptyValueElements() {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT pair_id FROM autofill WHERE TRIM(value)= \"\""));
  if (!s.is_valid())
    return false;

  std::set<int64> ids;
  while (s.Step())
    ids.insert(s.ColumnInt64(0));
  if (!s.Succeeded())
    return false;

  bool success = true;
  for (std::set<int64>::const_iterator iter = ids.begin(); iter != ids.end();
       ++iter) {
    if (!RemoveFormElementForID(*iter))
      success = false;
  }

  return success;
}

bool AutofillTable::GetAllAutofillEntries(std::vector<AutofillEntry>* entries) {
  DCHECK(entries);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT name, value, date_created FROM autofill a JOIN "
      "autofill_dates ad ON a.pair_id=ad.pair_id"));

  bool first_entry = true;
  AutofillKey* current_key_ptr = NULL;
  std::vector<Time>* timestamps_ptr = NULL;
  string16 name, value;
  Time time;
  while (s.Step()) {
    name = s.ColumnString16(0);
    value = s.ColumnString16(1);
    time = Time::FromTimeT(s.ColumnInt64(2));

    if (first_entry) {
      current_key_ptr = new AutofillKey(name, value);

      timestamps_ptr = new std::vector<Time>;
      timestamps_ptr->push_back(time);

      first_entry = false;
    } else {
      // we've encountered the next entry
      if (current_key_ptr->name().compare(name) != 0 ||
          current_key_ptr->value().compare(value) != 0) {
        AutofillEntry entry(*current_key_ptr, *timestamps_ptr);
        entries->push_back(entry);

        delete current_key_ptr;
        delete timestamps_ptr;

        current_key_ptr = new AutofillKey(name, value);
        timestamps_ptr = new std::vector<Time>;
      }
      timestamps_ptr->push_back(time);
    }
  }

  // If there is at least one result returned, first_entry will be false.
  // For this case we need to do a final cleanup step.
  if (!first_entry) {
    AutofillEntry entry(*current_key_ptr, *timestamps_ptr);
    entries->push_back(entry);
    delete current_key_ptr;
    delete timestamps_ptr;
  }

  return s.Succeeded();
}

bool AutofillTable::GetAutofillTimestamps(const string16& name,
                                          const string16& value,
                                          std::vector<Time>* timestamps) {
  DCHECK(timestamps);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT date_created FROM autofill a JOIN "
      "autofill_dates ad ON a.pair_id=ad.pair_id "
      "WHERE a.name = ? AND a.value = ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);

  while (s.Step())
    timestamps->push_back(Time::FromTimeT(s.ColumnInt64(0)));

  return s.Succeeded();
}

bool AutofillTable::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& entries) {
  if (!entries.size())
    return true;

  // Remove all existing entries.
  for (size_t i = 0; i < entries.size(); i++) {
    std::string sql = "SELECT pair_id FROM autofill "
                      "WHERE name = ? AND value = ?";
    sql::Statement s(db_->GetUniqueStatement(sql.c_str()));
    s.BindString16(0, entries[i].key().name());
    s.BindString16(1, entries[i].key().value());

    if (!s.is_valid())
      return false;

    if (s.Step()) {
      if (!RemoveFormElementForID(s.ColumnInt64(0)))
        return false;
    }
  }

  // Insert all the supplied autofill entries.
  for (size_t i = 0; i < entries.size(); i++) {
    if (!InsertAutofillEntry(entries[i]))
      return false;
  }

  return true;
}

bool AutofillTable::InsertAutofillEntry(const AutofillEntry& entry) {
  std::string sql = "INSERT INTO autofill (name, value, value_lower, count) "
                    "VALUES (?, ?, ?, ?)";
  sql::Statement s(db_->GetUniqueStatement(sql.c_str()));
  s.BindString16(0, entry.key().name());
  s.BindString16(1, entry.key().value());
  s.BindString16(2, base::i18n::ToLower(entry.key().value()));
  s.BindInt(3, entry.timestamps().size());

  if (!s.Run())
    return false;

  int64 pair_id = db_->GetLastInsertRowId();
  for (size_t i = 0; i < entry.timestamps().size(); i++) {
    if (!InsertPairIDAndDate(pair_id, entry.timestamps()[i]))
      return false;
  }

  return true;
}

bool AutofillTable::AddFormFieldValueTime(const FormField& element,
                                          std::vector<AutofillChange>* changes,
                                          Time time) {
  int count = 0;
  int64 pair_id;

  if (!GetIDAndCountOfFormElement(element, &pair_id, &count))
    return false;

  if (count == 0 && !InsertFormElement(element, &pair_id))
    return false;

  if (!SetCountOfFormElement(pair_id, count + 1))
    return false;

  // If we already have more than 2 times delete last one, before adding new
  // one.
  if (count >= 2 && !DeleteLastAccess(pair_id))
    return false;

  if (!InsertPairIDAndDate(pair_id, time))
    return false;

  AutofillChange::Type change_type =
      count == 0 ? AutofillChange::ADD : AutofillChange::UPDATE;
  changes->push_back(
      AutofillChange(change_type,
                     AutofillKey(element.name, element.value)));
  return true;
}


bool AutofillTable::RemoveFormElement(const string16& name,
                                      const string16& value) {
  // Find the id for that pair.
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT pair_id FROM autofill WHERE  name = ? AND value= ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);

  if (s.Step())
    return RemoveFormElementForID(s.ColumnInt64(0));
  return false;
}

bool AutofillTable::AddAutofillProfile(const AutofillProfile& profile) {
  if (IsAutofillGUIDInTrash(profile.guid()))
    return true;

  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO autofill_profiles"
      "(guid, company_name, address_line_1, address_line_2, city, state,"
      " zipcode, country, country_code, date_modified)"
      "VALUES (?,?,?,?,?,?,?,?,?,?)"));
  BindAutofillProfileToStatement(profile, &s);

  if (!s.Run())
    return false;

  return AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::GetAutofillProfile(const std::string& guid,
                                       AutofillProfile** profile) {
  DCHECK(base::IsValidGUID(guid));
  DCHECK(profile);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, company_name, address_line_1, address_line_2, city, state,"
      " zipcode, country, country_code, date_modified "
      "FROM autofill_profiles "
      "WHERE guid=?"));
  s.BindString(0, guid);

  if (!s.Step())
    return false;

  scoped_ptr<AutofillProfile> p(AutofillProfileFromStatement(s));

  // Get associated name info.
  AddAutofillProfileNamesToProfile(db_, p.get());

  // Get associated email info.
  AddAutofillProfileEmailsToProfile(db_, p.get());

  // Get associated phone info.
  AddAutofillProfilePhonesToProfile(db_, p.get());

  *profile = p.release();
  return true;
}

bool AutofillTable::GetAutofillProfiles(
    std::vector<AutofillProfile*>* profiles) {
  DCHECK(profiles);
  profiles->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles"));

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    AutofillProfile* profile = NULL;
    if (!GetAutofillProfile(guid, &profile))
      return false;
    profiles->push_back(profile);
  }

  return s.Succeeded();
}

bool AutofillTable::UpdateAutofillProfile(const AutofillProfile& profile) {
  DCHECK(base::IsValidGUID(profile.guid()));

  // Don't update anything until the trash has been emptied.  There may be
  // pending modifications to process.
  if (!IsAutofillProfilesTrashEmpty())
    return true;

  AutofillProfile* tmp_profile = NULL;
  if (!GetAutofillProfile(profile.guid(), &tmp_profile))
    return false;

  // Preserve appropriate modification dates by not updating unchanged profiles.
  scoped_ptr<AutofillProfile> old_profile(tmp_profile);
  if (old_profile->Compare(profile) == 0)
    return true;

  AutofillProfile new_profile(profile);
  std::vector<string16> values;

  old_profile->GetMultiInfo(NAME_FULL, &values);
  values[0] = new_profile.GetInfo(NAME_FULL);
  new_profile.SetMultiInfo(NAME_FULL, values);

  old_profile->GetMultiInfo(EMAIL_ADDRESS, &values);
  values[0] = new_profile.GetInfo(EMAIL_ADDRESS);
  new_profile.SetMultiInfo(EMAIL_ADDRESS, values);

  old_profile->GetMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  values[0] = new_profile.GetInfo(PHONE_HOME_WHOLE_NUMBER);
  new_profile.SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, values);

  return UpdateAutofillProfileMulti(new_profile);
}

bool AutofillTable::UpdateAutofillProfileMulti(const AutofillProfile& profile) {
  DCHECK(base::IsValidGUID(profile.guid()));

  // Don't update anything until the trash has been emptied.  There may be
  // pending modifications to process.
  if (!IsAutofillProfilesTrashEmpty())
    return true;

  AutofillProfile* tmp_profile = NULL;
  if (!GetAutofillProfile(profile.guid(), &tmp_profile))
    return false;

  // Preserve appropriate modification dates by not updating unchanged profiles.
  scoped_ptr<AutofillProfile> old_profile(tmp_profile);
  if (old_profile->Compare(profile) == 0)
    return true;

  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE autofill_profiles "
      "SET guid=?, company_name=?, address_line_1=?, address_line_2=?, "
      "    city=?, state=?, zipcode=?, country=?, country_code=?, "
      "    date_modified=? "
      "WHERE guid=?"));
  BindAutofillProfileToStatement(profile, &s);
  s.BindString(10, profile.guid());

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  if (!result)
    return result;

  // Remove the old names, emails, and phone numbers.
  if (!RemoveAutofillProfilePieces(profile.guid(), db_))
    return false;

  return AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::RemoveAutofillProfile(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));

  if (IsAutofillGUIDInTrash(guid)) {
    sql::Statement s_trash(db_->GetUniqueStatement(
        "DELETE FROM autofill_profiles_trash WHERE guid = ?"));
    s_trash.BindString(0, guid);

    bool success = s_trash.Run();
    DCHECK_GT(db_->GetLastChangeCount(), 0) << "Expected item in trash";
    return success;
  }

  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles WHERE guid = ?"));
  s.BindString(0, guid);

  if (!s.Run())
    return false;

  return RemoveAutofillProfilePieces(guid, db_);
}

bool AutofillTable::ClearAutofillProfiles() {
  sql::Statement s1(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles"));

  if (!s1.Run())
    return false;

  sql::Statement s2(db_->GetUniqueStatement(
      "DELETE FROM autofill_profile_names"));

  if (!s2.Run())
    return false;

  sql::Statement s3(db_->GetUniqueStatement(
      "DELETE FROM autofill_profile_emails"));

  if (!s3.Run())
    return false;

  sql::Statement s4(db_->GetUniqueStatement(
      "DELETE FROM autofill_profile_phones"));

  return s4.Run();
}

bool AutofillTable::AddCreditCard(const CreditCard& credit_card) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO credit_cards"
      "(guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified)"
      "VALUES (?,?,?,?,?,?)"));
  BindCreditCardToStatement(credit_card, &s);

  if (!s.Run())
    return false;

  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return true;
}

bool AutofillTable::GetCreditCard(const std::string& guid,
                                  CreditCard** credit_card) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards "
      "WHERE guid = ?"));
  s.BindString(0, guid);

  if (!s.Step())
    return false;

  *credit_card = CreditCardFromStatement(s);
  return true;
}

bool AutofillTable::GetCreditCards(
    std::vector<CreditCard*>* credit_cards) {
  DCHECK(credit_cards);
  credit_cards->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM credit_cards"));

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    CreditCard* credit_card = NULL;
    if (!GetCreditCard(guid, &credit_card))
      return false;
    credit_cards->push_back(credit_card);
  }

  return s.Succeeded();
}

bool AutofillTable::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK(base::IsValidGUID(credit_card.guid()));

  CreditCard* tmp_credit_card = NULL;
  if (!GetCreditCard(credit_card.guid(), &tmp_credit_card))
    return false;

  // Preserve appropriate modification dates by not updating unchanged cards.
  scoped_ptr<CreditCard> old_credit_card(tmp_credit_card);
  if (*old_credit_card == credit_card)
    return true;

  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE credit_cards "
      "SET guid=?, name_on_card=?, expiration_month=?, "
      "    expiration_year=?, card_number_encrypted=?, date_modified=? "
      "WHERE guid=?"));
  BindCreditCardToStatement(credit_card, &s);
  s.BindString(6, credit_card.guid());

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool AutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM credit_cards WHERE guid = ?"));
  s.BindString(0, guid);

  return s.Run();
}

bool AutofillTable::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end,
    std::vector<std::string>* profile_guids,
    std::vector<std::string>* credit_card_guids) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t =
      (delete_end.is_null() || delete_end == base::Time::Max()) ?
          std::numeric_limits<time_t>::max() : delete_end.ToTimeT();

  // Remember Autofill profiles in the time range.
  sql::Statement s_profiles_get(db_->GetUniqueStatement(
      "SELECT guid FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_profiles_get.BindInt64(0, delete_begin_t);
  s_profiles_get.BindInt64(1, delete_end_t);

  profile_guids->clear();
  while (s_profiles_get.Step()) {
    std::string guid = s_profiles_get.ColumnString(0);
    profile_guids->push_back(guid);
  }
  if (!s_profiles_get.Succeeded())
    return false;

  // Remove Autofill profiles in the time range.
  sql::Statement s_profiles(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_profiles.BindInt64(0, delete_begin_t);
  s_profiles.BindInt64(1, delete_end_t);

  if (!s_profiles.Run())
    return false;

  // Remember Autofill credit cards in the time range.
  sql::Statement s_credit_cards_get(db_->GetUniqueStatement(
      "SELECT guid FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_credit_cards_get.BindInt64(0, delete_begin_t);
  s_credit_cards_get.BindInt64(1, delete_end_t);

  credit_card_guids->clear();
  while (s_credit_cards_get.Step()) {
    std::string guid = s_credit_cards_get.ColumnString(0);
    credit_card_guids->push_back(guid);
  }
  if (!s_credit_cards_get.Succeeded())
    return false;

  // Remove Autofill credit cards in the time range.
  sql::Statement s_credit_cards(db_->GetUniqueStatement(
      "DELETE FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_credit_cards.BindInt64(0, delete_begin_t);
  s_credit_cards.BindInt64(1, delete_end_t);

  return s_credit_cards.Run();
}

bool AutofillTable::GetAutofillProfilesInTrash(
    std::vector<std::string>* guids) {
  guids->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles_trash"));

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    guids->push_back(guid);
  }

  return s.Succeeded();
}

bool AutofillTable::EmptyAutofillProfilesTrash() {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles_trash"));

  return s.Run();
}


bool AutofillTable::RemoveFormElementForID(int64 pair_id) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill WHERE pair_id = ?"));
  s.BindInt64(0, pair_id);

  if (s.Run())
    return RemoveFormElementForTimeRange(pair_id, Time(), Time(), NULL);

  return false;
}

bool AutofillTable::AddAutofillGUIDToTrash(const std::string& guid) {
  sql::Statement s(db_->GetUniqueStatement(
    "INSERT INTO autofill_profiles_trash"
    " (guid) "
    "VALUES (?)"));
  s.BindString(0, guid);

  return s.Run();
}

bool AutofillTable::IsAutofillProfilesTrashEmpty() {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles_trash"));

  return !s.Step();
}

bool AutofillTable::IsAutofillGUIDInTrash(const std::string& guid) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles_trash "
      "WHERE guid = ?"));
  s.BindString(0, guid);

  return s.Step();
}

bool AutofillTable::InitMainTable() {
  if (!db_->DoesTableExist("autofill")) {
    if (!db_->Execute("CREATE TABLE autofill ("
                      "name VARCHAR, "
                      "value VARCHAR, "
                      "value_lower VARCHAR, "
                      "pair_id INTEGER PRIMARY KEY, "
                      "count INTEGER DEFAULT 1)")) {
      NOTREACHED();
      return false;
    }
    if (!db_->Execute("CREATE INDEX autofill_name ON autofill (name)")) {
       NOTREACHED();
       return false;
    }
    if (!db_->Execute("CREATE INDEX autofill_name_value_lower ON "
                      "autofill (name, value_lower)")) {
       NOTREACHED();
       return false;
    }
  }
  return true;
}

bool AutofillTable::InitCreditCardsTable() {
  if (!db_->DoesTableExist("credit_cards")) {
    if (!db_->Execute("CREATE TABLE credit_cards ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "name_on_card VARCHAR, "
                      "expiration_month INTEGER, "
                      "expiration_year INTEGER, "
                      "card_number_encrypted BLOB, "
                      "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }

  return true;
}

bool AutofillTable::InitDatesTable() {
  if (!db_->DoesTableExist("autofill_dates")) {
    if (!db_->Execute("CREATE TABLE autofill_dates ( "
                      "pair_id INTEGER DEFAULT 0, "
                      "date_created INTEGER DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
    if (!db_->Execute("CREATE INDEX autofill_dates_pair_id ON "
                      "autofill_dates (pair_id)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfilesTable() {
  if (!db_->DoesTableExist("autofill_profiles")) {
    if (!db_->Execute("CREATE TABLE autofill_profiles ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "company_name VARCHAR, "
                      "address_line_1 VARCHAR, "
                      "address_line_2 VARCHAR, "
                      "city VARCHAR, "
                      "state VARCHAR, "
                      "zipcode VARCHAR, "
                      "country VARCHAR, "
                      "country_code VARCHAR, "
                      "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfileNamesTable() {
  if (!db_->DoesTableExist("autofill_profile_names")) {
    if (!db_->Execute("CREATE TABLE autofill_profile_names ( "
                      "guid VARCHAR, "
                      "first_name VARCHAR, "
                      "middle_name VARCHAR, "
                      "last_name VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfileEmailsTable() {
  if (!db_->DoesTableExist("autofill_profile_emails")) {
    if (!db_->Execute("CREATE TABLE autofill_profile_emails ( "
                      "guid VARCHAR, "
                      "email VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfilePhonesTable() {
  if (!db_->DoesTableExist("autofill_profile_phones")) {
    if (!db_->Execute("CREATE TABLE autofill_profile_phones ( "
                      "guid VARCHAR, "
                      "type INTEGER DEFAULT 0, "
                      "number VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfileTrashTable() {
  if (!db_->DoesTableExist("autofill_profiles_trash")) {
    if (!db_->Execute("CREATE TABLE autofill_profiles_trash ( "
                      "guid VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

// Add the card_number_encrypted column if credit card table was not
// created in this build (otherwise the column already exists).
// WARNING: Do not change the order of the execution of the SQL
// statements in this case!  Profile corruption and data migration
// issues WILL OCCUR. See http://crbug.com/10913
//
// The problem is that if a user has a profile which was created before
// r37036, when the credit_cards table was added, and then failed to
// update this profile between the credit card addition and the addition
// of the "encrypted" columns (44963), the next data migration will put
// the user's profile in an incoherent state: The user will update from
// a data profile set to be earlier than 22, and therefore pass through
// this update case.  But because the user did not have a credit_cards
// table before starting Chrome, it will have just been initialized
// above, and so already have these columns -- and thus this data
// update step will have failed.
//
// The false assumption in this case is that at this step in the
// migration, the user has a credit card table, and that this
// table does not include encrypted columns!
// Because this case does not roll back the complete set of SQL
// transactions properly in case of failure (that is, it does not
// roll back the table initialization done above), the incoherent
// profile will now see itself as being at version 22 -- but include a
// fully initialized credit_cards table.  Every time Chrome runs, it
// will try to update the web database and fail at this step, unless
// we allow for the faulty assumption described above by checking for
// the existence of the columns only AFTER we've executed the commands
// to add them.
bool AutofillTable::MigrateToVersion23AddCardNumberEncryptedColumn() {
  if (!db_->DoesColumnExist("credit_cards", "card_number_encrypted")) {
    if (!db_->Execute("ALTER TABLE credit_cards ADD COLUMN "
                      "card_number_encrypted BLOB DEFAULT NULL")) {
      LOG(WARNING) << "Could not add card_number_encrypted to "
                      "credit_cards table.";
      return false;
    }
  }

  if (!db_->DoesColumnExist("credit_cards", "verification_code_encrypted")) {
    if (!db_->Execute("ALTER TABLE credit_cards ADD COLUMN "
                      "verification_code_encrypted BLOB DEFAULT NULL")) {
      LOG(WARNING) << "Could not add verification_code_encrypted to "
                      "credit_cards table.";
      return false;
    }
  }

  return true;
}

// One-time cleanup for http://crbug.com/38364 - In the presence of
// multi-byte UTF-8 characters, that bug could cause Autofill strings
// to grow larger and more corrupt with each save.  The cleanup removes
// any row with a string field larger than a reasonable size.  The string
// fields examined here are precisely the ones that were subject to
// corruption by the original bug.
bool AutofillTable::MigrateToVersion24CleanupOversizedStringFields() {
  const std::string autofill_is_too_big =
      "max(length(name), length(value)) > 500";

  const std::string credit_cards_is_too_big =
      "max(length(label), length(name_on_card), length(type), "
      "    length(expiration_month), length(expiration_year), "
      "    length(billing_address), length(shipping_address) "
      ") > 500";

  const std::string autofill_profiles_is_too_big =
      "max(length(label), length(first_name), "
      "    length(middle_name), length(last_name), length(email), "
      "    length(company_name), length(address_line_1), "
      "    length(address_line_2), length(city), length(state), "
      "    length(zipcode), length(country), length(phone)) > 500";

  std::string query = "DELETE FROM autofill_dates WHERE pair_id IN ("
      "SELECT pair_id FROM autofill WHERE " + autofill_is_too_big + ")";

  if (!db_->Execute(query.c_str()))
    return false;

  query = "DELETE FROM autofill WHERE " + autofill_is_too_big;

  if (!db_->Execute(query.c_str()))
    return false;

  // Only delete from legacy credit card tables where specific columns exist.
  if (db_->DoesColumnExist("credit_cards", "label") &&
      db_->DoesColumnExist("credit_cards", "name_on_card") &&
      db_->DoesColumnExist("credit_cards", "type") &&
      db_->DoesColumnExist("credit_cards", "expiration_month") &&
      db_->DoesColumnExist("credit_cards", "expiration_year") &&
      db_->DoesColumnExist("credit_cards", "billing_address") &&
      db_->DoesColumnExist("credit_cards", "shipping_address") &&
      db_->DoesColumnExist("autofill_profiles", "label")) {
    query = "DELETE FROM credit_cards WHERE (" + credit_cards_is_too_big +
        ") OR label IN (SELECT label FROM autofill_profiles WHERE " +
        autofill_profiles_is_too_big + ")";

    if (!db_->Execute(query.c_str()))
      return false;
  }

  if (db_->DoesColumnExist("autofill_profiles", "label")) {
    query = "DELETE FROM autofill_profiles WHERE " +
        autofill_profiles_is_too_big;

    if (!db_->Execute(query.c_str()))
      return false;
  }

  return true;
}

// Change the credit_cards.billing_address column from a string to an
// int.  The stored string is the label of an address, so we have to
// select the unique ID of this address using the label as a foreign
// key into the |autofill_profiles| table.
bool AutofillTable::MigrateToVersion27UpdateLegacyCreditCards() {
  // Only migrate from legacy credit card tables where specific columns
  // exist.
  if (!(db_->DoesColumnExist("credit_cards", "unique_id") &&
        db_->DoesColumnExist("credit_cards", "billing_address") &&
        db_->DoesColumnExist("autofill_profiles", "unique_id"))) {
    return true;
  }

  std::string stmt =
      "SELECT credit_cards.unique_id, autofill_profiles.unique_id "
      "FROM autofill_profiles, credit_cards "
      "WHERE credit_cards.billing_address = autofill_profiles.label";
  sql::Statement s(db_->GetUniqueStatement(stmt.c_str()));

  std::map<int, int> cc_billing_map;
  while (s.Step())
    cc_billing_map[s.ColumnInt(0)] = s.ColumnInt(1);
  if (!s.Succeeded())
    return false;

  // Windows already stores the IDs as strings in |billing_address|. Try
  // to convert those.
  if (cc_billing_map.empty()) {
    std::string stmt = "SELECT unique_id,billing_address FROM credit_cards";
    sql::Statement s(db_->GetUniqueStatement(stmt.c_str()));

    while (s.Step()) {
      int id = 0;
      if (base::StringToInt(s.ColumnString(1), &id))
        cc_billing_map[s.ColumnInt(0)] = id;
    }
    if (!s.Succeeded())
      return false;
  }

  if (!db_->Execute("CREATE TABLE credit_cards_temp ( "
                    "label VARCHAR, "
                    "unique_id INTEGER PRIMARY KEY, "
                    "name_on_card VARCHAR, "
                    "type VARCHAR, "
                    "card_number VARCHAR, "
                    "expiration_month INTEGER, "
                    "expiration_year INTEGER, "
                    "verification_code VARCHAR, "
                    "billing_address INTEGER, "
                    "shipping_address VARCHAR, "
                    "card_number_encrypted BLOB, "
                    "verification_code_encrypted BLOB)")) {
    return false;
  }

  if (!db_->Execute(
      "INSERT INTO credit_cards_temp "
      "SELECT label,unique_id,name_on_card,type,card_number,"
      "expiration_month,expiration_year,verification_code,0,"
      "shipping_address,card_number_encrypted,"
      "verification_code_encrypted FROM credit_cards")) {
    return false;
  }

  if (!db_->Execute("DROP TABLE credit_cards"))
    return false;

  if (!db_->Execute("ALTER TABLE credit_cards_temp RENAME TO credit_cards"))
    return false;

  for (std::map<int, int>::const_iterator iter = cc_billing_map.begin();
       iter != cc_billing_map.end(); ++iter) {
    sql::Statement s(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE credit_cards SET billing_address=? WHERE unique_id=?"));
    s.BindInt(0, (*iter).second);
    s.BindInt(1, (*iter).first);

    if (!s.Run())
      return false;
  }

  return true;
}

bool AutofillTable::MigrateToVersion30AddDateModifed() {
  // Add date_modified to autofill_profiles.
  if (!db_->DoesColumnExist("autofill_profiles", "date_modified")) {
    if (!db_->Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                      "date_modified INTEGER NON NULL DEFAULT 0")) {
      return false;
    }

    sql::Statement s(db_->GetUniqueStatement(
        "UPDATE autofill_profiles SET date_modified=?"));
    s.BindInt64(0, Time::Now().ToTimeT());

    if (!s.Run())
      return false;
  }

  // Add date_modified to credit_cards.
  if (!db_->DoesColumnExist("credit_cards", "date_modified")) {
    if (!db_->Execute("ALTER TABLE credit_cards ADD COLUMN "
                      "date_modified INTEGER NON NULL DEFAULT 0")) {
      return false;
    }

    sql::Statement s(db_->GetUniqueStatement(
        "UPDATE credit_cards SET date_modified=?"));
    s.BindInt64(0, Time::Now().ToTimeT());

    if (!s.Run())
      return false;
  }

  return true;
}

bool AutofillTable::MigrateToVersion31AddGUIDToCreditCardsAndProfiles() {
  // Note that we need to check for the guid column's existence due to the
  // fact that for a version 22 database the |autofill_profiles| table
  // gets created fresh with |InitAutofillProfilesTable|.
  if (!db_->DoesColumnExist("autofill_profiles", "guid")) {
    if (!db_->Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                      "guid VARCHAR NOT NULL DEFAULT \"\"")) {
      return false;
    }

    // Set all the |guid| fields to valid values.

    sql::Statement s(db_->GetUniqueStatement("SELECT unique_id "
                                             "FROM autofill_profiles"));

    while (s.Step()) {
      sql::Statement update_s(
          db_->GetUniqueStatement("UPDATE autofill_profiles "
                                  "SET guid=? WHERE unique_id=?"));
      update_s.BindString(0, base::GenerateGUID());
      update_s.BindInt(1, s.ColumnInt(0));

      if (!update_s.Run())
        return false;
    }
    if (!s.Succeeded())
      return false;
  }

  // Note that we need to check for the guid column's existence due to the
  // fact that for a version 22 database the |autofill_profiles| table
  // gets created fresh with |InitAutofillProfilesTable|.
  if (!db_->DoesColumnExist("credit_cards", "guid")) {
    if (!db_->Execute("ALTER TABLE credit_cards ADD COLUMN "
                      "guid VARCHAR NOT NULL DEFAULT \"\"")) {
      return false;
    }

    // Set all the |guid| fields to valid values.

    sql::Statement s(db_->GetUniqueStatement("SELECT unique_id "
                                             "FROM credit_cards"));

    while (s.Step()) {
      sql::Statement update_s(
          db_->GetUniqueStatement("UPDATE credit_cards "
                                  "set guid=? WHERE unique_id=?"));
      update_s.BindString(0, base::GenerateGUID());
      update_s.BindInt(1, s.ColumnInt(0));

      if (!update_s.Run())
        return false;
    }
    if (!s.Succeeded())
      return false;
  }

  return true;
}

bool AutofillTable::MigrateToVersion32UpdateProfilesAndCreditCards() {
  if (db_->DoesColumnExist("autofill_profiles", "unique_id")) {
    if (!db_->Execute("CREATE TABLE autofill_profiles_temp ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "label VARCHAR, "
                      "first_name VARCHAR, "
                      "middle_name VARCHAR, "
                      "last_name VARCHAR, "
                      "email VARCHAR, "
                      "company_name VARCHAR, "
                      "address_line_1 VARCHAR, "
                      "address_line_2 VARCHAR, "
                      "city VARCHAR, "
                      "state VARCHAR, "
                      "zipcode VARCHAR, "
                      "country VARCHAR, "
                      "phone VARCHAR, "
                      "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      return false;
    }

    if (!db_->Execute(
        "INSERT INTO autofill_profiles_temp "
        "SELECT guid, label, first_name, middle_name, last_name, email, "
        "company_name, address_line_1, address_line_2, city, state, "
        "zipcode, country, phone, date_modified "
        "FROM autofill_profiles")) {
      return false;
    }

    if (!db_->Execute("DROP TABLE autofill_profiles"))
      return false;

    if (!db_->Execute(
        "ALTER TABLE autofill_profiles_temp RENAME TO autofill_profiles")) {
      return false;
    }
  }

  if (db_->DoesColumnExist("credit_cards", "unique_id")) {
    if (!db_->Execute("CREATE TABLE credit_cards_temp ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "label VARCHAR, "
                      "name_on_card VARCHAR, "
                      "expiration_month INTEGER, "
                      "expiration_year INTEGER, "
                      "card_number_encrypted BLOB, "
                      "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      return false;
    }

    if (!db_->Execute(
        "INSERT INTO credit_cards_temp "
        "SELECT guid, label, name_on_card, expiration_month, "
        "expiration_year, card_number_encrypted, date_modified "
        "FROM credit_cards")) {
      return false;
    }

    if (!db_->Execute("DROP TABLE credit_cards"))
      return false;

    if (!db_->Execute("ALTER TABLE credit_cards_temp RENAME TO credit_cards"))
      return false;
  }

  return true;
}

// Test the existence of the |first_name| column as an indication that
// we need a migration.  It is possible that the new |autofill_profiles|
// schema is in place because the table was newly created when migrating
// from a pre-version-22 database.
bool AutofillTable::MigrateToVersion33ProfilesBasedOnFirstName() {
  if (db_->DoesColumnExist("autofill_profiles", "first_name")) {
    // Create autofill_profiles_temp table that will receive the data.
    if (!db_->DoesTableExist("autofill_profiles_temp")) {
      if (!db_->Execute("CREATE TABLE autofill_profiles_temp ( "
                        "guid VARCHAR PRIMARY KEY, "
                        "company_name VARCHAR, "
                        "address_line_1 VARCHAR, "
                        "address_line_2 VARCHAR, "
                        "city VARCHAR, "
                        "state VARCHAR, "
                        "zipcode VARCHAR, "
                        "country VARCHAR, "
                        "date_modified INTEGER NOT NULL DEFAULT 0)")) {
        return false;
      }
    }

    sql::Statement s(db_->GetUniqueStatement(
        "SELECT guid, first_name, middle_name, last_name, email, "
        "company_name, address_line_1, address_line_2, city, state, "
        "zipcode, country, phone, date_modified "
        "FROM autofill_profiles"));

    while (s.Step()) {
      AutofillProfile profile;
      profile.set_guid(s.ColumnString(0));
      DCHECK(base::IsValidGUID(profile.guid()));

      profile.SetInfo(NAME_FIRST, s.ColumnString16(1));
      profile.SetInfo(NAME_MIDDLE, s.ColumnString16(2));
      profile.SetInfo(NAME_LAST, s.ColumnString16(3));
      profile.SetInfo(EMAIL_ADDRESS, s.ColumnString16(4));
      profile.SetInfo(COMPANY_NAME, s.ColumnString16(5));
      profile.SetInfo(ADDRESS_HOME_LINE1, s.ColumnString16(6));
      profile.SetInfo(ADDRESS_HOME_LINE2, s.ColumnString16(7));
      profile.SetInfo(ADDRESS_HOME_CITY, s.ColumnString16(8));
      profile.SetInfo(ADDRESS_HOME_STATE, s.ColumnString16(9));
      profile.SetInfo(ADDRESS_HOME_ZIP, s.ColumnString16(10));
      profile.SetInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(11));
      profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, s.ColumnString16(12));
      int64 date_modified = s.ColumnInt64(13);

      sql::Statement s_insert(db_->GetUniqueStatement(
          "INSERT INTO autofill_profiles_temp"
          "(guid, company_name, address_line_1, address_line_2, city,"
          " state, zipcode, country, date_modified)"
          "VALUES (?,?,?,?,?,?,?,?,?)"));
      s_insert.BindString(0, profile.guid());
      s_insert.BindString16(1, profile.GetInfo(COMPANY_NAME));
      s_insert.BindString16(2, profile.GetInfo(ADDRESS_HOME_LINE1));
      s_insert.BindString16(3, profile.GetInfo(ADDRESS_HOME_LINE2));
      s_insert.BindString16(4, profile.GetInfo(ADDRESS_HOME_CITY));
      s_insert.BindString16(5, profile.GetInfo(ADDRESS_HOME_STATE));
      s_insert.BindString16(6, profile.GetInfo(ADDRESS_HOME_ZIP));
      s_insert.BindString16(7, profile.GetInfo(ADDRESS_HOME_COUNTRY));
      s_insert.BindInt64(8, date_modified);

      if (!s_insert.Run())
        return false;

      // Add the other bits: names, emails, and phone numbers.
      if (!AddAutofillProfilePieces(profile, db_))
        return false;
    }  // endwhile
    if (!s.Succeeded())
      return false;

    if (!db_->Execute("DROP TABLE autofill_profiles"))
      return false;

    if (!db_->Execute(
        "ALTER TABLE autofill_profiles_temp RENAME TO autofill_profiles")) {
      return false;
    }
  }

  // Remove the labels column from the credit_cards table.
  if (db_->DoesColumnExist("credit_cards", "label")) {
    if (!db_->Execute("CREATE TABLE credit_cards_temp ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "name_on_card VARCHAR, "
                      "expiration_month INTEGER, "
                      "expiration_year INTEGER, "
                      "card_number_encrypted BLOB, "
                      "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      return false;
    }

    if (!db_->Execute(
        "INSERT INTO credit_cards_temp "
        "SELECT guid, name_on_card, expiration_month, "
        "expiration_year, card_number_encrypted, date_modified "
        "FROM credit_cards")) {
      return false;
    }

    if (!db_->Execute("DROP TABLE credit_cards"))
      return false;

    if (!db_->Execute("ALTER TABLE credit_cards_temp RENAME TO credit_cards"))
      return false;
  }

  return true;
}

// Test the existence of the |country_code| column as an indication that
// we need a migration.  It is possible that the new |autofill_profiles|
// schema is in place because the table was newly created when migrating
// from a pre-version-22 database.
bool AutofillTable::MigrateToVersion34ProfilesBasedOnCountryCode() {
  if (!db_->DoesColumnExist("autofill_profiles", "country_code")) {
    if (!db_->Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                      "country_code VARCHAR")) {
      return false;
    }

    // Set all the |country_code| fields to match existing |country| values.
    sql::Statement s(db_->GetUniqueStatement("SELECT guid, country "
                                             "FROM autofill_profiles"));

    while (s.Step()) {
      sql::Statement update_s(
          db_->GetUniqueStatement("UPDATE autofill_profiles "
                                  "SET country_code=? WHERE guid=?"));

      string16 country = s.ColumnString16(1);
      std::string app_locale = AutofillCountry::ApplicationLocale();
      update_s.BindString(0, AutofillCountry::GetCountryCode(country,
                                                             app_locale));
      update_s.BindString(1, s.ColumnString(0));

      if (!update_s.Run())
        return false;
    }
    if (!s.Succeeded())
      return false;
  }

  return true;
}

// Correct all country codes with value "UK" to be "GB".  This data
// was mistakenly introduced in build 686.0.  This migration is to clean
// it up.  See http://crbug.com/74511 for details.
bool AutofillTable::MigrateToVersion35GreatBritainCountryCodes() {
  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE autofill_profiles SET country_code=\"GB\" "
      "WHERE country_code=\"UK\""));

  return s.Run();
}

// Merge and cull older profiles where possible.
bool AutofillTable::MigrateToVersion37MergeAndCullOlderProfiles() {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, date_modified FROM autofill_profiles"));

  // Accumulate the good profiles.
  std::vector<AutofillProfile> accumulated_profiles;
  std::vector<AutofillProfile*> accumulated_profiles_p;
  std::map<std::string, int64> modification_map;
  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    int64 date_modified = s.ColumnInt64(1);
    modification_map.insert(
        std::pair<std::string, int64>(guid, date_modified));
    AutofillProfile* profile = NULL;
    if (!GetAutofillProfile(guid, &profile))
      return false;

    scoped_ptr<AutofillProfile> p(profile);

    if (PersonalDataManager::IsValidLearnableProfile(*p)) {
      std::vector<AutofillProfile> merged_profiles;
      bool merged = PersonalDataManager::MergeProfile(
          *p, accumulated_profiles_p, &merged_profiles);

      std::swap(accumulated_profiles, merged_profiles);

      accumulated_profiles_p.clear();
      accumulated_profiles_p.resize(accumulated_profiles.size());
      std::transform(accumulated_profiles.begin(),
                     accumulated_profiles.end(),
                     accumulated_profiles_p.begin(),
                     address_of<AutofillProfile>);

      // If the profile got merged trash the original.
      if (merged)
        AddAutofillGUIDToTrash(p->guid());

    } else {
      // An invalid profile, so trash it.
      AddAutofillGUIDToTrash(p->guid());
    }
  }  // endwhile
  if (!s.Succeeded())
    return false;

  // Drop the current profiles.
  if (!ClearAutofillProfiles())
    return false;

  // Add the newly merged profiles back in.
  for (std::vector<AutofillProfile>::const_iterator
          iter = accumulated_profiles.begin();
       iter != accumulated_profiles.end();
       ++iter) {
    if (!AddAutofillProfile(*iter))
      return false;

    // Fix up the original modification date.
    std::map<std::string, int64>::const_iterator date_item =
        modification_map.find(iter->guid());
    if (date_item == modification_map.end())
      return false;

    sql::Statement s_date(db_->GetUniqueStatement(
        "UPDATE autofill_profiles SET date_modified=? "
        "WHERE guid=?"));
    s_date.BindInt64(0, date_item->second);
    s_date.BindString(1, iter->guid());

    if (!s_date.Run())
      return false;
  }

  return true;
}
