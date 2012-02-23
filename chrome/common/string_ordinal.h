// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_STRING_ORDINAL_H_
#define CHROME_COMMON_STRING_ORDINAL_H_
#pragma once

#include <string>

// A StringOrdinal represents a specially-formatted string that can be used
// for ordering. The StringOrdinal class has an unbounded dense strict total
// order, which mean for any StringOrdinals a, b and c:
//
//  - a < b and b < c implies a < c (transitivity);
//  - exactly one of a < b, b < a and a = b holds (trichotomy);
//  - if a < b, there is a StringOrdinal x such that a < x < b (density);
//  - there are StringOrdinals x and y such that x < a < y (unboundedness).
//
// This means that when StringOrdinal is used for sorting a list, if
// any item changes its position in the list, only its StringOrdinal value
// has to change to represent the new order, and all the other older values
// can stay the same.
class StringOrdinal {
 public:
  // Creates a StringOrdinal from the given string. It may be valid or invalid.
  explicit StringOrdinal(const std::string& string_ordinal);

  // Creates an invalid StringOrdinal.
  StringOrdinal();

  // Creates a valid initial StringOrdinal, this is called to create the first
  // element of StringOrdinal list (i.e. before we have any other values we can
  // generate from).
  static StringOrdinal CreateInitialOrdinal();

  bool IsValid() const;

  // All remaining functions can only be called if IsValid() holds.
  // It is an error to call them if IsValid() is false.

  // Order-related Functions

  // Returns true iff |*this| < |other|.
  bool LessThan(const StringOrdinal& other) const;

  // Returns true iff |*this| > |other|.
  bool GreaterThan(const StringOrdinal& other) const;

  // Returns true iff |*this| == |other| (i.e. |*this| < |other| and
  // |other| < |*this| are both false).
  bool Equal(const StringOrdinal& other) const;

  // Returns true iff |*this| == |other| or |*this| and |other|
  // are both invalid.
  bool EqualOrBothInvalid(const StringOrdinal& other) const;

  // Given |*this| != |other|, returns a StringOrdinal x such that
  // min(|*this|, |other|) < x < max(|*this|, |other|). It is an error
  // to call this function when |*this| == |other|.
  StringOrdinal CreateBetween(const StringOrdinal& other) const;

  // Returns a StringOrdinal |x| such that |x| < |*this|.
  StringOrdinal CreateBefore() const;

  // Returns a StringOrdinal |x| such that |*this| < |x|.
  StringOrdinal CreateAfter() const;

  // It is guaranteed that a StringOrdinal constructed from the returned
  // string will be valid.
  std::string ToString() const;

  // Do this so we can use std::find on a std::vector of StringOrdinals.
  bool operator==(const StringOrdinal& rhs) const;

  // Use of copy constructor and default assignment for this class is allowed.

 private:
  // The string representation of the StringOrdinal.
  std::string string_ordinal_;

  // The validity of the StringOrdinal (i.e., is it of the format [a-z]*[b-z]),
  // created to cache validity to prevent frequent recalculations.
  bool is_valid_;
};

// A helper class that can be used by STL containers that require sorting.
class StringOrdinalLessThan {
 public:
  bool operator() (const StringOrdinal& lhs, const StringOrdinal& rhs) const;
};

#endif  // CHROME_COMMON_STRING_ORDINAL_H_
