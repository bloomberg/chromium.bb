// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_table.h"

#include "app/sql/statement.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_util.h"
#include "chrome/common/guid.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/form_field.h"

using base::Time;
using webkit_glue::FormField;

namespace {
typedef std::vector<Tuple3<int64, string16, string16> > AutofillElementList;
}  // anonymous namespace

bool AutofillTable::Init() {
 return (InitMainTable() && InitCreditCardsTable() && InitDatesTable() &&
     InitProfilesTable() && InitProfileNamesTable() &&
     InitProfileEmailsTable() && InitProfilePhonesTable() &&
     InitProfileTrashTable());
}

bool AutofillTable::AddFormFieldValues(const std::vector<FormField>& elements,
                                       std::vector<AutofillChange>* changes) {
  return AddFormFieldValuesTime(elements, changes, Time::Now());
}

bool AutofillTable::AddFormFieldValue(const FormField& element,
                                      std::vector<AutofillChange>* changes) {
  return AddFormFieldValueTime(element, changes, base::Time::Now());
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
    if (!s) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString16(0, name);
    s.BindInt(1, limit);
  } else {
    string16 prefix_lower = l10n_util::ToLower(prefix);
    string16 next_prefix = prefix_lower;
    next_prefix[next_prefix.length() - 1]++;

    s.Assign(db_->GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? AND "
        "value_lower >= ? AND "
        "value_lower < ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    if (!s) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

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
    base::Time delete_begin,
    base::Time delete_end,
    std::vector<AutofillChange>* changes) {
  DCHECK(changes);
  // Query for the pair_id, name, and value of all form elements that
  // were used between the given times.
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT DISTINCT a.pair_id, a.name, a.value "
      "FROM autofill_dates ad JOIN autofill a ON ad.pair_id = a.pair_id "
      "WHERE ad.date_created >= ? AND ad.date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1,
              delete_end.is_null() ?
                  std::numeric_limits<int64>::max() :
                  delete_end.ToTimeT());

  AutofillElementList elements;
  while (s.Step()) {
    elements.push_back(MakeTuple(s.ColumnInt64(0),
                                 s.ColumnString16(1),
                                 s.ColumnString16(2)));
  }

  if (!s.Succeeded()) {
    NOTREACHED();
    return false;
  }

  for (AutofillElementList::iterator itr = elements.begin();
       itr != elements.end(); itr++) {
    int how_many = 0;
    if (!RemoveFormElementForTimeRange(itr->a, delete_begin, delete_end,
                                       &how_many)) {
      return false;
    }
    bool was_removed = false;
    if (!AddToCountOfFormElement(itr->a, -how_many, &was_removed))
      return false;
    AutofillChange::Type change_type =
        was_removed ? AutofillChange::REMOVE : AutofillChange::UPDATE;
    changes->push_back(AutofillChange(change_type,
                                      AutofillKey(itr->b, itr->c)));
  }

  return true;
}

bool AutofillTable::RemoveFormElementForTimeRange(int64 pair_id,
                                                  const Time delete_begin,
                                                  const Time delete_end,
                                                  int* how_many) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_dates WHERE pair_id = ? AND "
      "date_created >= ? AND date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindInt64(0, pair_id);
  s.BindInt64(1, delete_begin.is_null() ? 0 : delete_begin.ToTimeT());
  s.BindInt64(2, delete_end.is_null() ? std::numeric_limits<int64>::max() :
                                        delete_end.ToTimeT());

  bool result = s.Run();
  if (how_many)
    *how_many = db_->GetLastChangeCount();

  return result;
}

bool AutofillTable::AddToCountOfFormElement(int64 pair_id,
                                            int delta,
                                            bool* was_removed) {
  DCHECK(was_removed);
  int count = 0;
  *was_removed = false;

  if (!GetCountOfFormElement(pair_id, &count))
    return false;

  if (count + delta == 0) {
    if (!RemoveFormElementForID(pair_id))
      return false;
    *was_removed = true;
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
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT pair_id, count FROM autofill "
      "WHERE name = ? AND value = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, element.name);
  s.BindString16(1, element.value);

  *pair_id = 0;
  *count = 0;

  if (s.Step()) {
    *pair_id = s.ColumnInt64(0);
    *count = s.ColumnInt(1);
  }

  return true;
}

bool AutofillTable::GetCountOfFormElement(int64 pair_id, int* count) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT count FROM autofill WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt(0, count);
  s.BindInt64(1, pair_id);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool AutofillTable::InsertFormElement(const FormField& element,
                                      int64* pair_id) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO autofill (name, value, value_lower) VALUES (?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, element.name);
  s.BindString16(1, element.value);
  s.BindString16(2, l10n_util::ToLower(element.value));

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  *pair_id = db_->GetLastInsertRowId();
  return true;
}

bool AutofillTable::InsertPairIDAndDate(int64 pair_id,
                                        base::Time date_created) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO autofill_dates "
      "(pair_id, date_created) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt64(0, pair_id);
  s.BindInt64(1, date_created.ToTimeT());

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool AutofillTable::AddFormFieldValuesTime(
    const std::vector<FormField>& elements,
    std::vector<AutofillChange>* changes,
    base::Time time) {
  // Only add one new entry for each unique element name.  Use |seen_names| to
  // track this.  Add up to |kMaximumUniqueNames| unique entries per form.
  const size_t kMaximumUniqueNames = 256;
  std::set<string16> seen_names;
  bool result = true;
  for (std::vector<FormField>::const_iterator
       itr = elements.begin();
       itr != elements.end();
       itr++) {
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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::set<int64> ids;
  while (s.Step())
    ids.insert(s.ColumnInt64(0));

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

  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  bool first_entry = true;
  AutofillKey* current_key_ptr = NULL;
  std::vector<base::Time>* timestamps_ptr = NULL;
  string16 name, value;
  base::Time time;
  while (s.Step()) {
    name = s.ColumnString16(0);
    value = s.ColumnString16(1);
    time = Time::FromTimeT(s.ColumnInt64(2));

    if (first_entry) {
      current_key_ptr = new AutofillKey(name, value);

      timestamps_ptr = new std::vector<base::Time>;
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
        timestamps_ptr = new std::vector<base::Time>;
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
                                          std::vector<base::Time>* timestamps) {
  DCHECK(timestamps);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT date_created FROM autofill a JOIN "
      "autofill_dates ad ON a.pair_id=ad.pair_id "
      "WHERE a.name = ? AND a.value = ?"));

  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, name);
  s.BindString16(1, value);
  while (s.Step()) {
    timestamps->push_back(Time::FromTimeT(s.ColumnInt64(0)));
  }

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
    if (!s.is_valid()) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString16(0, entries[i].key().name());
    s.BindString16(1, entries[i].key().value());
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
  if (!s.is_valid()) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, entry.key().name());
  s.BindString16(1, entry.key().value());
  s.BindString16(2, l10n_util::ToLower(entry.key().value()));
  s.BindInt(3, entry.timestamps().size());

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  int64 pair_id = db_->GetLastInsertRowId();
  for (size_t i = 0; i < entry.timestamps().size(); i++) {
    if (!InsertPairIDAndDate(pair_id, entry.timestamps()[i]))
      return false;
  }

  return true;
}

bool AutofillTable::AddFormFieldValueTime(const FormField& element,
                                          std::vector<AutofillChange>* changes,
                                          base::Time time) {
  int count = 0;
  int64 pair_id;

  if (!GetIDAndCountOfFormElement(element, &pair_id, &count))
    return false;

  if (count == 0 && !InsertFormElement(element, &pair_id))
    return false;

  if (!SetCountOfFormElement(pair_id, count + 1))
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
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  autofill_util::BindAutofillProfileToStatement(profile, &s);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  if (!s.Succeeded())
    return false;

  return autofill_util::AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::GetAutofillProfile(const std::string& guid,
                                       AutofillProfile** profile) {
  DCHECK(guid::IsValidGUID(guid));
  DCHECK(profile);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, company_name, address_line_1, address_line_2, city, state,"
      " zipcode, country, country_code, date_modified "
      "FROM autofill_profiles "
      "WHERE guid=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  if (!s.Step())
    return false;

  if (!s.Succeeded())
    return false;

  scoped_ptr<AutofillProfile> p(autofill_util::AutofillProfileFromStatement(s));

  // Get associated name info.
  autofill_util::AddAutofillProfileNamesToProfile(db_, p.get());

  // Get associated email info.
  autofill_util::AddAutofillProfileEmailsToProfile(db_, p.get());

  // Get associated phone info.
  autofill_util::AddAutofillProfilePhonesToProfile(db_, p.get());

  // Get associated fax info.
  autofill_util::AddAutofillProfileFaxesToProfile(db_, p.get());

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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

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
  DCHECK(guid::IsValidGUID(profile.guid()));

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

  old_profile->GetMultiInfo(PHONE_FAX_WHOLE_NUMBER, &values);
  values[0] = new_profile.GetInfo(PHONE_FAX_WHOLE_NUMBER);
  new_profile.SetMultiInfo(PHONE_FAX_WHOLE_NUMBER, values);

  return UpdateAutofillProfileMulti(new_profile);
}

bool AutofillTable::UpdateAutofillProfileMulti(const AutofillProfile& profile) {
  DCHECK(guid::IsValidGUID(profile.guid()));

  // Don't update anything until the trash has been emptied.  There may be
  // pending modifications to process.
  if (!IsAutofillProfilesTrashEmpty())
    return true;

  AutofillProfile* tmp_profile = NULL;
  if (!GetAutofillProfile(profile.guid(), &tmp_profile))
    return false;

  // Preserve appropriate modification dates by not updating unchanged profiles.
  scoped_ptr<AutofillProfile> old_profile(tmp_profile);
  if (old_profile->CompareMulti(profile) == 0)
    return true;

  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE autofill_profiles "
      "SET guid=?, company_name=?, address_line_1=?, address_line_2=?, "
      "    city=?, state=?, zipcode=?, country=?, country_code=?, "
      "    date_modified=? "
      "WHERE guid=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  autofill_util::BindAutofillProfileToStatement(profile, &s);
  s.BindString(10, profile.guid());
  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  if (!result)
    return result;

  // Remove the old names, emails, and phone/fax numbers.
  if (!autofill_util::RemoveAutofillProfilePieces(profile.guid(), db_))
    return false;

  return autofill_util::AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::RemoveAutofillProfile(const std::string& guid) {
  DCHECK(guid::IsValidGUID(guid));

  if (IsAutofillGUIDInTrash(guid)) {
    sql::Statement s_trash(db_->GetUniqueStatement(
        "DELETE FROM autofill_profiles_trash WHERE guid = ?"));
    if (!s_trash) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }
    s_trash.BindString(0, guid);
    if (!s_trash.Run()) {
      NOTREACHED() << "Expected item in trash.";
      return false;
    }

    return true;
  }

  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  if (!s.Run())
    return false;

  return autofill_util::RemoveAutofillProfilePieces(guid, db_);
}

bool AutofillTable::ClearAutofillProfiles() {
  sql::Statement s1(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles"));
  if (!s1) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  if (!s1.Run())
    return false;

  sql::Statement s2(db_->GetUniqueStatement(
      "DELETE FROM autofill_profile_names"));
  if (!s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  if (!s2.Run())
    return false;

  sql::Statement s3(db_->GetUniqueStatement(
      "DELETE FROM autofill_profile_emails"));
  if (!s3) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  if (!s3.Run())
    return false;

  sql::Statement s4(db_->GetUniqueStatement(
      "DELETE FROM autofill_profile_phones"));
  if (!s4) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  if (!s4.Run())
    return false;

  return true;
}

bool AutofillTable::AddCreditCard(const CreditCard& credit_card) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO credit_cards"
      "(guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified)"
      "VALUES (?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  autofill_util::BindCreditCardToStatement(credit_card, &s);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return s.Succeeded();
}

bool AutofillTable::GetCreditCard(const std::string& guid,
                                  CreditCard** credit_card) {
  DCHECK(guid::IsValidGUID(guid));
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards "
      "WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  if (!s.Step())
    return false;

  *credit_card = autofill_util::CreditCardFromStatement(s);

  return s.Succeeded();
}

bool AutofillTable::GetCreditCards(
    std::vector<CreditCard*>* credit_cards) {
  DCHECK(credit_cards);
  credit_cards->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM credit_cards"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

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
  DCHECK(guid::IsValidGUID(credit_card.guid()));

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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  autofill_util::BindCreditCardToStatement(credit_card, &s);
  s.BindString(6, credit_card.guid());
  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool AutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(guid::IsValidGUID(guid));
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM credit_cards WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  return s.Run();
}

bool AutofillTable::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = delete_end.is_null() ?
      std::numeric_limits<time_t>::max() :
      delete_end.ToTimeT();

  // Remove Autofill profiles in the time range.
  sql::Statement s_profiles(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  if (!s_profiles) {
    NOTREACHED() << "Autofill profiles statement prepare failed";
    return false;
  }

  s_profiles.BindInt64(0, delete_begin_t);
  s_profiles.BindInt64(1, delete_end_t);
  s_profiles.Run();

  if (!s_profiles.Succeeded()) {
    NOTREACHED();
    return false;
  }

  // Remove Autofill profiles in the time range.
  sql::Statement s_credit_cards(db_->GetUniqueStatement(
      "DELETE FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  if (!s_credit_cards) {
    NOTREACHED() << "Autofill credit cards statement prepare failed";
    return false;
  }

  s_credit_cards.BindInt64(0, delete_begin_t);
  s_credit_cards.BindInt64(1, delete_end_t);
  s_credit_cards.Run();

  if (!s_credit_cards.Succeeded()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool AutofillTable::GetAutofillProfilesInTrash(
    std::vector<std::string>* guids) {
  guids->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles_trash"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    guids->push_back(guid);
  }

  return s.Succeeded();
}

bool AutofillTable::EmptyAutofillProfilesTrash() {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles_trash"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  return s.Run();
}


bool AutofillTable::RemoveFormElementForID(int64 pair_id) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, pair_id);
  if (s.Run()) {
    return RemoveFormElementForTimeRange(pair_id, base::Time(), base::Time(),
                                         NULL);
  }
  return false;
}

bool AutofillTable::AddAutofillGUIDToTrash(const std::string& guid) {
  sql::Statement s(db_->GetUniqueStatement(
    "INSERT INTO autofill_profiles_trash"
    " (guid) "
    "VALUES (?)"));
  if (!s) {
    NOTREACHED();
    return sql::INIT_FAILURE;
  }

  s.BindString(0, guid);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool AutofillTable::IsAutofillProfilesTrashEmpty() {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles_trash"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  return !s.Step();
}

bool AutofillTable::IsAutofillGUIDInTrash(const std::string& guid) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles_trash "
      "WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

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
