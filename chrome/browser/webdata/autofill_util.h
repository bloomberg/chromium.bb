// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_UTIL_H_
#define CHROME_BROWSER_WEBDATA_AUTOFILL_UTIL_H_
#pragma once

#include "app/sql/statement.h"
#include "base/string16.h"

class AutofillProfile;
class CreditCard;

// Constants for the |autofill_profile_phones| |type| column.
enum AutofillPhoneType {
  kAutofillPhoneNumber = 0,
  kAutofillFaxNumber = 1
};

namespace autofill_util {

// TODO(jhawkins): This is a temporary stop-gap measure designed to prevent
// a malicious site from DOS'ing the browser with extremely large profile
// data.  The correct solution is to parse this data asynchronously.
// See http://crbug.com/49332.
string16 LimitDataSize(const string16& data);

void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    sql::Statement* s);

AutofillProfile* AutofillProfileFromStatement(const sql::Statement& s);

void BindCreditCardToStatement(const CreditCard& credit_card,
                               sql::Statement* s);

CreditCard* CreditCardFromStatement(const sql::Statement& s);

bool AddAutofillProfileNamesToProfile(sql::Connection* db,
                                      AutofillProfile* profile);

bool AddAutofillProfileEmailsToProfile(sql::Connection* db,
                                       AutofillProfile* profile);

bool AddAutofillProfilePhonesToProfile(sql::Connection* db,
                                       AutofillProfile* profile);

bool AddAutofillProfileFaxesToProfile(sql::Connection* db,
                                      AutofillProfile* profile);

bool AddAutofillProfileNames(const AutofillProfile& profile,
                             sql::Connection* db);

bool AddAutofillProfileEmails(const AutofillProfile& profile,
                              sql::Connection* db);

bool AddAutofillProfilePhones(const AutofillProfile& profile,
                              AutofillPhoneType phone_type,
                              sql::Connection* db);

bool AddAutofillProfilePieces(const AutofillProfile& profile,
                              sql::Connection* db);

bool RemoveAutofillProfilePieces(const std::string& guid, sql::Connection* db);

}  // namespace autofill_util

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_UTIL_H_
