// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"

class AutofillField;
class AutofillScanner;

// Represents a logical form field in a web form.  Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormField {
 public:
  virtual ~FormField() {}

  // Classifies each field in |fields| with its heuristically detected type.
  // The association is stored into |map|.  Each field has a derived unique name
  // that is used as the key into the |map|.
  static void ParseFormFields(const std::vector<AutofillField*>& fields,
                              FieldTypeMap* map);

 protected:
  // A bit-field used for matching specific parts of a field in question.
  enum MatchType {
    MATCH_LABEL  = 1 << 0,
    MATCH_NAME   = 1 << 1,
    MATCH_ALL    = MATCH_LABEL | MATCH_NAME
  };

  // Only derived classes may instantiate.
  FormField() {}

  // Attempts to parse a form field with the given pattern.  Returns true on
  // success and fills |match| with a pointer to the field.
  static bool ParseField(AutofillScanner* scanner,
                         const string16& pattern,
                         const AutofillField** match);

  // Parses the stream of fields in |scanner| with regular expression |pattern|
  // as specified in the |match_type| bit field (see |MatchType|).  If |match|
  // is non-NULL and the pattern matches, the matched field is returned.
  // A |true| result is returned in the case of a successful match, false
  // otherwise.
  static bool ParseFieldSpecifics(AutofillScanner* scanner,
                                  const string16& pattern,
                                  int match_type,
                                  const AutofillField** match);

  // Attempts to parse a field with an empty label.  Returns true
  // on success and fills |match| with a pointer to the field.
  static bool ParseEmptyLabel(AutofillScanner* scanner,
                              const AutofillField** match);

  // Adds an association between a field and a type to |map|.
  static bool AddClassification(const AutofillField* field,
                                AutofillFieldType type,
                                FieldTypeMap* map);

  // Derived classes must implement this interface to supply field type
  // information.  |ParseFormFields| coordinates the parsing and extraction
  // of types from an input vector of |AutofillField| objects and delegates
  // the type extraction via this method.
  virtual bool ClassifyField(FieldTypeMap* map) const = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(FormFieldTest, Match);

  // Matches the regular expression |pattern| against the components of |field|
  // as specified in the |match_type| bit field (see |MatchType|).
  static bool Match(const AutofillField* field,
                    const string16& pattern,
                    int match_type);

  DISALLOW_COPY_AND_ASSIGN(FormField);
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
