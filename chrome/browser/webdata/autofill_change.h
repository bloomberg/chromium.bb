// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#pragma once

#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/webdata/autofill_entry.h"

class AutoFillProfile;
class CreditCard;

// For classic Autofill form fields, the KeyType is AutofillKey.
// Autofill++ types such as AutoFillProfile and CreditCard simply use an int.
template <typename KeyType>
class GenericAutofillChange {
 public:
  typedef enum {
    ADD,
    UPDATE,
    REMOVE
  } Type;

  virtual ~GenericAutofillChange() {}

  Type type() const { return type_; }
  const KeyType& key() const { return key_; }

 protected:
  GenericAutofillChange(Type type, const KeyType& key)
      : type_(type),
        key_(key) {}
 private:
  Type type_;
  KeyType key_;
};

class AutofillChange : public GenericAutofillChange<AutofillKey> {
 public:
  AutofillChange(Type t, const AutofillKey& k)
      : GenericAutofillChange<AutofillKey>(t, k) {}
  bool operator==(const AutofillChange& change) const {
    return type() == change.type() && key() == change.key();
  }
};

class AutofillProfileChange : public GenericAutofillChange<string16> {
 public:
  // If t == REMOVE, |p| should be NULL. |pre_update_label| only applies to
  // UPDATE changes.
  AutofillProfileChange(Type t, string16 k, const AutoFillProfile* p,
      const string16& pre_update_label)
      : GenericAutofillChange<string16>(t, k), profile_(p),
        pre_update_label_(pre_update_label) {}

  const AutoFillProfile* profile() const { return profile_; }
  const string16& pre_update_label() const { return pre_update_label_; }
  bool operator==(const AutofillProfileChange& change) const {
    if (type() != change.type() || key() != change.key())
      return false;
    if (type() == REMOVE)
      return true;
    // TODO(dhollowa): Replace with |AutoFillProfile::Compare|.
    // http://crbug.com/58813
    if (*profile() != *change.profile())
      return false;
    return type() == ADD || pre_update_label_ == change.pre_update_label();
  }
 private:
  const AutoFillProfile* profile_;  // Unowned pointer, can be NULL.
  const string16 pre_update_label_;
};

class AutofillCreditCardChange : public GenericAutofillChange<string16> {
 public:
  // If t == REMOVE, |card| should be NULL.
  AutofillCreditCardChange(Type t, string16 k, const CreditCard* card)
      : GenericAutofillChange<string16>(t, k), credit_card_(card) {}

  const CreditCard* credit_card() const { return credit_card_; }
  bool operator==(const AutofillCreditCardChange& change) const {
    return type() == change.type() && key() == change.key() &&
           (type() != REMOVE) ? *credit_card() == *change.credit_card() : true;
 }
 private:
  const CreditCard* credit_card_;  // Unowned pointer, can be NULL.
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
