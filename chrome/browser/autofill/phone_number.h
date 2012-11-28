// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/autofill/phone_number_i18n.h"

class AutofillProfile;

// A form group that stores phone number information.
class PhoneNumber : public FormGroup {
 public:
  explicit PhoneNumber(AutofillProfile* profile);
  PhoneNumber(const PhoneNumber& number);
  virtual ~PhoneNumber();

  PhoneNumber& operator=(const PhoneNumber& number);

  void set_profile(AutofillProfile* profile) { profile_ = profile; }

  // FormGroup implementation:
  virtual void GetMatchingTypes(const string16& text,
                                FieldTypeSet* matching_types) const OVERRIDE;
  virtual string16 GetRawInfo(AutofillFieldType type) const OVERRIDE;
  virtual void SetRawInfo(AutofillFieldType type,
                          const string16& value) OVERRIDE;
  virtual string16 GetCanonicalizedInfo(AutofillFieldType type) const OVERRIDE;
  virtual bool SetCanonicalizedInfo(AutofillFieldType type,
                                    const string16& value) OVERRIDE;

  // Size and offset of the prefix and suffix portions of phone numbers.
  static const size_t kPrefixOffset = 0;
  static const size_t kPrefixLength = 3;
  static const size_t kSuffixOffset = 3;
  static const size_t kSuffixLength = 4;

  // The class used to combine home phone parts into a whole number.
  class PhoneCombineHelper {
   public:
    PhoneCombineHelper();
    ~PhoneCombineHelper();

    // If |type| is a phone field type, saves the |value| accordingly and
    // returns true.  For all other field types returs false.
    bool SetInfo(AutofillFieldType type, const string16& value);

    // Returns true if parsing was successful, false otherwise.
    bool ParseNumber(const std::string& region, string16* value);

    // Returns true if both |phone_| and |whole_number_| are empty.
    bool IsEmpty() const;

   private:
    string16 country_;
    string16 city_;
    string16 phone_;
    string16 whole_number_;
  };

 private:
  // FormGroup:
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const OVERRIDE;

  // Validates |number_| and translates it into digits-only format.
  bool NormalizePhone();

  // Returns the region code for this phone number, which is an ISO 3166
  // 2-letter country code.  The name "region" is chosen since "country code"
  // already refers to part of a phone number.  The returned value is based on
  // the |profile_|.
  std::string GetRegion() const;

  // Updates the cached parsed number if the profile's region has changed
  // since the last time the cache was updated.
  void UpdateCacheIfNeeded() const;

  // The phone number.
  string16 number_;
  // Profile which stores the region used as hint when normalizing the number.
  AutofillProfile* profile_;  // WEAK

  // Cached number.
  mutable autofill_i18n::PhoneObject cached_parsed_phone_;
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
