// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_L10N_UTIL_COLLATOR_H_
#define APP_L10N_UTIL_COLLATOR_H_
#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "unicode/coll.h"

namespace l10n_util {

// Compares the two strings using the specified collator.
UCollationResult CompareStringWithCollator(const icu::Collator* collator,
                                           const std::wstring& lhs,
                                           const std::wstring& rhs);
UCollationResult CompareString16WithCollator(const icu::Collator* collator,
                                             const string16& lhs,
                                             const string16& rhs);

// Used by SortStringsUsingMethod. Invokes a method on the objects passed to
// operator (), comparing the string results using a collator.
template <class T, class Method>
class StringMethodComparatorWithCollator
    : public std::binary_function<const std::wstring&,
                                  const std::wstring&,
                                  bool> {
 public:
  StringMethodComparatorWithCollator(icu::Collator* collator, Method method)
      : collator_(collator),
        method_(method) { }

  // Returns true if lhs preceeds rhs.
  bool operator() (T* lhs_t, T* rhs_t) {
    return CompareStringWithCollator(collator_, (lhs_t->*method_)(),
                                     (rhs_t->*method_)()) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
  Method method_;
};

// Used by SortStringsUsingMethod. Invokes a method on the objects passed to
// operator (), comparing the string results using <.
template <class T, class Method>
class StringMethodComparator : public std::binary_function<const std::wstring&,
                                                           const std::wstring&,
                                                           bool> {
 public:
  explicit StringMethodComparator(Method method) : method_(method) { }

  // Returns true if lhs preceeds rhs.
  bool operator() (T* lhs_t, T* rhs_t) {
    return (lhs_t->*method_)() < (rhs_t->*method_)();
  }

 private:
  Method method_;
};

// Sorts the objects in |elements| using the method |method|, which must return
// a string. Sorting is done using a collator, unless a collator can not be
// found in which case the strings are sorted using the operator <.
template <class T, class Method>
void SortStringsUsingMethod(const std::wstring& locale,
                            std::vector<T*>* elements,
                            Method method) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(WideToUTF8(locale).c_str());
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(loc, error));
  if (U_FAILURE(error)) {
    sort(elements->begin(), elements->end(),
         StringMethodComparator<T, Method>(method));
    return;
  }

  std::sort(elements->begin(), elements->end(),
      StringMethodComparatorWithCollator<T, Method>(collator.get(), method));
}

// Compares two elements' string keys and returns true if the first element's
// string key is less than the second element's string key. The Element must
// have a method like the follow format to return the string key.
// const std::wstring& GetStringKey() const;
// This uses the locale specified in the constructor.
template <class Element>
class StringComparator : public std::binary_function<const Element&,
                                                     const Element&,
                                                     bool> {
 public:
  explicit StringComparator(icu::Collator* collator)
      : collator_(collator) { }

  // Returns true if lhs precedes rhs.
  bool operator()(const Element& lhs, const Element& rhs) {
    const std::wstring& lhs_string_key = lhs.GetStringKey();
    const std::wstring& rhs_string_key = rhs.GetStringKey();

    return StringComparator<std::wstring>(collator_)(lhs_string_key,
                                                     rhs_string_key);
  }

 private:
  icu::Collator* collator_;
};

// Specialization of operator() method for std::wstring version.
template <>
bool StringComparator<std::wstring>::operator()(const std::wstring& lhs,
                                                const std::wstring& rhs);

#if !defined(WCHAR_T_IS_UTF16)
// Specialization of operator() method for string16 version.
template <>
bool StringComparator<string16>::operator()(const string16& lhs,
                                            const string16& rhs);
#endif // !defined(WCHAR_T_IS_UTF16)

// In place sorting of |elements| of a vector according to the string key of
// each element in the vector by using collation rules for |locale|.
// |begin_index| points to the start position of elements in the vector which
// want to be sorted. |end_index| points to the end position of elements in the
// vector which want to be sorted
template <class Element>
void SortVectorWithStringKey(const std::string& locale,
                             std::vector<Element>* elements,
                             unsigned int begin_index,
                             unsigned int end_index,
                             bool needs_stable_sort) {
  DCHECK(begin_index < end_index &&
         end_index <= static_cast<unsigned int>(elements->size()));
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(locale.c_str());
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(loc, error));
  if (U_FAILURE(error))
    collator.reset();
  StringComparator<Element> c(collator.get());
  if (needs_stable_sort) {
    stable_sort(elements->begin() + begin_index,
                elements->begin() + end_index,
                c);
  } else {
    sort(elements->begin() + begin_index, elements->begin() + end_index, c);
  }
}

template <class Element>
void SortVectorWithStringKey(const std::string& locale,
                             std::vector<Element>* elements,
                             bool needs_stable_sort) {
  SortVectorWithStringKey<Element>(locale, elements, 0, elements->size(),
                                   needs_stable_sort);
}

}  // namespace l10n_util

#endif  // APP_L10N_UTIL_COLLATOR_H_
