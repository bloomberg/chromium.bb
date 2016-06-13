// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_COMPARATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_COMPARATOR_H_

#include <memory>
#include <set>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace autofill {

// A utility class to assist in the comparison of AutofillProfile data.
class AutofillProfileComparator {
 public:
  explicit AutofillProfileComparator(const base::StringPiece& app_locale);
  ~AutofillProfileComparator();

  enum WhitespaceSpec { RETAIN_WHITESPACE, DISCARD_WHITESPACE };

  // Returns a copy of |text| with uppercase converted to lowercase and
  // diacritics removed.
  //
  // If |whitespace_spec| is RETAIN_WHITESPACE, punctuation is converted to
  // spaces, and extraneous whitespace is trimmed and collapsed. For example,
  // "Jean- François" becomes "jean francois".
  //
  // If |whitespace_spec| is DISCARD_WHITESPACE, punctuation and whitespace are
  // discarded. For example, +1 (234) 567-8900 becomes 12345678900.
  base::string16 NormalizeForComparison(
      base::StringPiece16 text,
      WhitespaceSpec whitespace_spec = RETAIN_WHITESPACE) const;

  // Returns true if |p1| and |p2| are viable merge candidates. This means that
  // their names, addresses, email addreses, company names, and phone numbers
  // are all pairwise equivalent or mergeable.
  //
  // Note that mergeability is non-directional; merging two profiles will likely
  // incorporate data from both profiles.
  bool AreMergeable(const AutofillProfile& p1, const AutofillProfile& p2) const;

 protected:
  // Returns the set of unique tokens in |s|. Note that the string data backing
  // |s| is expected to have a lifetime which exceeds the call to UniqueTokens.
  static std::set<base::StringPiece16> UniqueTokens(base::StringPiece16 s);

  // Returns true if all of the tokens in |s1| are in |s2| or vice versa.
  static bool HaveSameTokens(base::StringPiece16 s1, base::StringPiece16 s2);

  // Generate the set of full/initial variants for |name_part|, where
  // |name_part| is the user's first or middle name. For example, given "jean
  // francois" (the normalized for comparison form of "Jean-François") this
  // function returns the set:
  //
  //   { "", "f", "francois,
  //     "j", "j f", "j francois",
  //     "jean", "jean f", "jean francois", "jf" }
  //
  // Note: Expects that |name| is already normalized for comparison.
  static std::set<base::string16> GetNamePartVariants(
      const base::string16& name_part);

  // Returns true if |full_name_2| is a variant of |full_name_1|.
  //
  // This function generates all variations of |full_name_1| and returns true if
  // one of these variants is equal to |full_name_2|. For example, this function
  // will return true if |full_name_2| is "john q public" and |full_name_1| is
  // "john quincy public" because |full_name_2| can be derived from
  // |full_name_1| by using the middle initial. Note that the reverse is not
  // true, "john quincy public" is not a name variant of "john q public".
  //
  // Note: Expects that |full_name| is already normalized for comparison.
  bool IsNameVariantOf(const base::string16& full_name_1,
                       const base::string16& full_name_2) const;

  // Returns true if |p1| and |p2| have names which are equivalent for the
  // purposes of merging the two profiles. This means one of the names is
  // empty, the names are the same, or one name is a variation of the other.
  // The name comparison is insensitive to case, punctuation and diacritics.
  //
  // Note that this method does not provide any guidance on actually merging
  // the names.
  bool HaveMergeableNames(const AutofillProfile& p1,
                          const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have email addresses which are equivalent for
  // the purposes of merging the two profiles. This means one of the email
  // addresses is empty, or the email addresses are the same (modulo case).
  //
  // Note that this method does not provide any guidance on actually merging
  // the email addresses.
  bool HaveMergeableEmailAddresses(const AutofillProfile& p1,
                                   const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have company names which are equivalent for
  // the purposes of merging the two profiles. This means one of the company
  // names is empty, or the normalized company names are the same (modulo case).
  //
  // Note that this method does not provide any guidance on actually merging
  // the company names.
  bool HaveMergeableCompanyNames(const AutofillProfile& p1,
                                 const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have phone numbers which are equivalent for
  // the purposes of merging the two profiles. This means one of the phone
  // numbers is empty, or the phone numbers match modulo formatting
  // differences or missing information. For example, if the phone numbers are
  // the same but one has an extension, country code, or area code and the other
  // does not.
  //
  // Note that this method does not provide any guidance on actually merging
  // the company names.
  bool HaveMergeablePhoneNumbers(const AutofillProfile& p1,
                                 const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have addresses which are equivalent for the
  // purposes of merging the two profiles. This means one of the addresses is
  // empty, or the addresses are a match. A number of normalization and
  // comparison heuristics are employed to determine if the addresses match.
  //
  // Note that this method does not provide any guidance on actually merging
  // the email addresses.
  bool HaveMergeableAddresses(const AutofillProfile& p1,
                              const AutofillProfile& p2) const;

 private:
  l10n::CaseInsensitiveCompare case_insensitive_compare_;
  std::unique_ptr<icu::Transliterator> transliterator_;
  const std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileComparator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_COMPARATOR_H_
