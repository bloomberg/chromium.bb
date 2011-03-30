// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_util.h"

#include "app/sql/statement.h"
#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/common/guid.h"

using base::Time;

namespace autofill_util {

// The maximum length allowed for form data.
const size_t kMaxDataLength = 1024;

string16 LimitDataSize(const string16& data) {
  if (data.size() > kMaxDataLength)
    return data.substr(0, kMaxDataLength);

  return data;
}

void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    sql::Statement* s) {
  DCHECK(guid::IsValidGUID(profile.guid()));
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
  DCHECK(guid::IsValidGUID(profile->guid()));

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
  DCHECK(guid::IsValidGUID(credit_card.guid()));
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
  DCHECK(guid::IsValidGUID(credit_card->guid()));

  credit_card->SetInfo(CREDIT_CARD_NAME, s.ColumnString16(1));
  credit_card->SetInfo(CREDIT_CARD_EXP_MONTH,
                       s.ColumnString16(2));
  credit_card->SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                       s.ColumnString16(3));
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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, profile->guid());

  std::vector<string16> first_names;
  std::vector<string16> middle_names;
  std::vector<string16> last_names;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    first_names.push_back(s.ColumnString16(1));
    middle_names.push_back(s.ColumnString16(2));
    last_names.push_back(s.ColumnString16(3));
  }
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
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, profile->guid());

  std::vector<string16> emails;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    emails.push_back(s.ColumnString16(1));
  }
  profile->SetMultiInfo(EMAIL_ADDRESS, emails);
  return true;
}

bool AddAutofillProfilePhonesToProfile(sql::Connection* db,
                                       AutofillProfile* profile) {
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, type, number "
      "FROM autofill_profile_phones "
      "WHERE guid=? AND type=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, profile->guid());
  s.BindInt(1, kAutofillPhoneNumber);

  std::vector<string16> numbers;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    numbers.push_back(s.ColumnString16(2));
  }
  profile->SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, numbers);
  return true;
}

bool AddAutofillProfileFaxesToProfile(sql::Connection* db,
                                      AutofillProfile* profile) {
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, type, number "
      "FROM autofill_profile_phones "
      "WHERE guid=? AND type=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, profile->guid());
  s.BindInt(1, kAutofillFaxNumber);

  std::vector<string16> numbers;
  while (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    numbers.push_back(s.ColumnString16(2));
  }
  profile->SetMultiInfo(PHONE_FAX_WHOLE_NUMBER, numbers);
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
    if (!s) {
      NOTREACHED();
      return false;
    }
    s.BindString(0, profile.guid());
    s.BindString16(1, first_names[i]);
    s.BindString16(2, middle_names[i]);
    s.BindString16(3, last_names[i]);

    if (!s.Run()) {
      NOTREACHED();
      return false;
    }
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
    if (!s) {
      NOTREACHED();
      return false;
    }
    s.BindString(0, profile.guid());
    s.BindString16(1, emails[i]);

    if (!s.Run()) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AddAutofillProfilePhones(const AutofillProfile& profile,
                              AutofillPhoneType phone_type,
                              sql::Connection* db) {
  AutofillFieldType field_type;
  if (phone_type == kAutofillPhoneNumber) {
    field_type = PHONE_HOME_WHOLE_NUMBER;
  } else if (phone_type == kAutofillFaxNumber) {
    field_type = PHONE_FAX_WHOLE_NUMBER;
  } else {
    NOTREACHED();
    return false;
  }

  std::vector<string16> numbers;
  profile.GetMultiInfo(field_type, &numbers);

  for (size_t i = 0; i < numbers.size(); ++i) {
    // Add the new number.
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_phones"
      " (guid, type, number) "
      "VALUES (?,?,?)"));
    if (!s) {
      NOTREACHED();
      return false;
    }
    s.BindString(0, profile.guid());
    s.BindInt(1, phone_type);
    s.BindString16(2, numbers[i]);

    if (!s.Run()) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AddAutofillProfilePieces(const AutofillProfile& profile,
                              sql::Connection* db) {
  if (!AddAutofillProfileNames(profile, db))
    return false;

  if (!AddAutofillProfileEmails(profile, db))
    return false;

  if (!AddAutofillProfilePhones(profile, kAutofillPhoneNumber, db))
    return false;

  if (!AddAutofillProfilePhones(profile, kAutofillFaxNumber, db))
    return false;

  return true;
}

bool RemoveAutofillProfilePieces(const std::string& guid, sql::Connection* db) {
  sql::Statement s1(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_names WHERE guid = ?"));
  if (!s1) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s1.BindString(0, guid);
  if (!s1.Run())
    return false;

  sql::Statement s2(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_emails WHERE guid = ?"));
  if (!s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s2.BindString(0, guid);
  if (!s2.Run())
    return false;

  sql::Statement s3(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_phones WHERE guid = ?"));
  if (!s3) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s3.BindString(0, guid);
  return s3.Run();
}

}  // namespace autofill_util
