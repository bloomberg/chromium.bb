// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#pragma once

#include "chrome/browser/webdata/autofill_entry.h"

class AutofillProfile;
class CreditCard;

// For classic Autofill form fields, the KeyType is AutofillKey.
// Autofill++ types such as AutofillProfile and CreditCard simply use an int.
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
  AutofillChange(Type type, const AutofillKey& key);
  virtual ~AutofillChange();
  bool operator==(const AutofillChange& change) const {
    return type() == change.type() && key() == change.key();
  }
};

// Change notification details for AutoFill profile changes.
class AutofillProfileChange : public GenericAutofillChange<std::string> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the GUID identifying the |profile|.
  // When |type| == ADD, |profile| should be non-NULL.
  // When |type| == UPDATE, |profile| should be non-NULL.
  // When |type| == REMOVE, |profile| should be NULL.
  AutofillProfileChange(Type type,
                        std::string key,
                        const AutofillProfile* profile);
  virtual ~AutofillProfileChange();

  const AutofillProfile* profile() const { return profile_; }
  bool operator==(const AutofillProfileChange& change) const;

 private:
  // Weak reference, can be NULL.
  const AutofillProfile* profile_;
};

// Change notification details for AutoFill credit card changes.
class AutofillCreditCardChange : public GenericAutofillChange<std::string> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the GUID identifying the |credit_card|.
  // When |type| == ADD, |credit_card| should be non-NULL.
  // When |type| == UPDATE, |credit_card| should be non-NULL.
  // When |type| == REMOVE, |credit_card| should be NULL.
  AutofillCreditCardChange(Type type,
                           std::string key,
                           const CreditCard* credit_card);
  virtual ~AutofillCreditCardChange();

  const CreditCard* credit_card() const { return credit_card_; }
  bool operator==(const AutofillCreditCardChange& change) const;

 private:
  // Weak reference, can be NULL.
  const CreditCard* credit_card_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
