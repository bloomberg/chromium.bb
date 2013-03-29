// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

#include <vector>

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

typedef std::vector<AutofillChange> AutofillChangeList;

// Change notification details for Autofill profile changes.
class AutofillProfileChange : public GenericAutofillChange<std::string> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key,
  // which is expected to be the GUID identifying the |profile|.
  // When |type| == ADD, |profile| should be non-NULL.
  // When |type| == UPDATE, |profile| should be non-NULL.
  // When |type| == REMOVE, |profile| should be NULL.
  AutofillProfileChange(Type type,
                        const std::string& key,
                        const AutofillProfile* profile);
  virtual ~AutofillProfileChange();

  const AutofillProfile* profile() const { return profile_; }
  bool operator==(const AutofillProfileChange& change) const;

 private:
  // Weak reference, can be NULL.
  const AutofillProfile* profile_;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
