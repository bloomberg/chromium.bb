// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#pragma once

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
  AutofillChange(Type type, const AutofillKey& key);
  virtual ~AutofillChange();
  bool operator==(const AutofillChange& change) const {
    return type() == change.type() && key() == change.key();
  }
};

// DEPRECATED
// TODO(dhollowa): Remove use of labels for sync.  http://crbug.com/58813
class AutofillProfileChange : public GenericAutofillChange<string16> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the label identifying the |profile|.
  // When |type| == ADD, |profile| should be non-NULL.
  // When |type| == UPDATE, |profile| should be non-NULL.
  // When |type| == REMOVE, |profile| should be NULL.
  // The |pre_update_label| input specifies the label as it was prior to the
  // change (applicable only for UPDATE).
  AutofillProfileChange(Type type,
                        string16 key,
                        const AutoFillProfile* profile,
                        const string16& pre_update_label);
  virtual ~AutofillProfileChange();

  const AutoFillProfile* profile() const { return profile_; }
  const string16& pre_update_label() const { return pre_update_label_; }
  bool operator==(const AutofillProfileChange& change) const;

 private:
  // Weak reference, can be NULL.
  const AutoFillProfile* profile_;
  const string16 pre_update_label_;
};

// DEPRECATED
// TODO(dhollowa): Remove use of labels for sync.  http://crbug.com/58813
class AutofillCreditCardChange : public GenericAutofillChange<string16> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the label identifying the |credit_card|.
  // When |type| == ADD, |credit_card| should be non-NULL.
  // When |type| == UPDATE, |credit_card| should be non-NULL.
  // When |type| == REMOVE, |credit_card| should be NULL.
  AutofillCreditCardChange(Type type,
                           string16 key,
                           const CreditCard* credit_card);
  virtual ~AutofillCreditCardChange();

  const CreditCard* credit_card() const { return credit_card_; }
  bool operator==(const AutofillCreditCardChange& change) const;

 private:
  // Weak reference, can be NULL.
  const CreditCard* credit_card_;
};

// Change notification details for AutoFill profile changes.
class AutofillProfileChangeGUID : public GenericAutofillChange<std::string> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the GUID identifying the |profile|.
  // When |type| == ADD, |profile| should be non-NULL.
  // When |type| == UPDATE, |profile| should be non-NULL.
  // When |type| == REMOVE, |profile| should be NULL.
  AutofillProfileChangeGUID(Type type,
                            std::string key,
                            const AutoFillProfile* profile);
  virtual ~AutofillProfileChangeGUID();

  const AutoFillProfile* profile() const { return profile_; }
  bool operator==(const AutofillProfileChangeGUID& change) const;

 private:
  // Weak reference, can be NULL.
  const AutoFillProfile* profile_;
};

// Change notification details for AutoFill credit card changes.
class AutofillCreditCardChangeGUID : public GenericAutofillChange<std::string> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the GUID identifying the |credit_card|.
  // When |type| == ADD, |credit_card| should be non-NULL.
  // When |type| == UPDATE, |credit_card| should be non-NULL.
  // When |type| == REMOVE, |credit_card| should be NULL.
  AutofillCreditCardChangeGUID(Type type,
                               std::string key,
                               const CreditCard* credit_card);
  virtual ~AutofillCreditCardChangeGUID();

  const CreditCard* credit_card() const { return credit_card_; }
  bool operator==(const AutofillCreditCardChangeGUID& change) const;

 private:
  // Weak reference, can be NULL.
  const CreditCard* credit_card_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
