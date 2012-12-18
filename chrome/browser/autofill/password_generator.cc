// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/password_generator.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/rand_util.h"

const int kMinUpper = 65;  // First upper case letter 'A'
const int kMaxUpper = 90;  // Last upper case letter 'Z'
const int kMinLower = 97;  // First lower case letter 'a'
const int kMaxLower = 122; // Last lower case letter 'z'
const int kMinDigit = 48;  // First digit '0'
const int kMaxDigit = 57;  // Last digit '9'
// Copy of the other printable symbols from the ASCII table since they are
// disjointed.
const char kOtherSymbols[] =
    {'!', '\"', '#', '$', '%', '&', '\'', '(',
     ')', '*', '+', ',', '-', '.', '/', ':',
     ';', '<', '=', '>', '?', '@', '[', '\\',
     ']', '^', '_', '`', '{', '|', '}', '~'};
const size_t kMinPasswordLength = 4;
const size_t kMaxPasswordLength = 15;

namespace {

// A helper function to get the length of the generated password from
// |max_length| retrieved from input password field.
size_t GetLengthFromHint(size_t max_length, size_t default_length) {
  if (max_length >= kMinPasswordLength && max_length <= kMaxPasswordLength)
    return max_length;
  else
    return default_length;
}

void InitializeAlphaNumericCharacters(std::vector<char>* characters) {
  for (int i = kMinDigit; i <= kMaxDigit; ++i)
    characters->push_back(static_cast<char>(i));
  for (int i = kMinUpper; i <= kMaxUpper; ++i)
    characters->push_back(static_cast<char>(i));
  for (int i = kMinLower; i <= kMaxLower; ++i)
    characters->push_back(static_cast<char>(i));
}

// Classic algorithm to randomly select |num_select| elements out of
// |num_total| elements. One description can be found at:
// "http://stackoverflow.com/questions/48087/select-a-random-n-elements-from-listt-in-c-sharp/48089#48089"
void GetRandomSelection(size_t num_to_select,
                        size_t num_total,
                        std::vector<size_t>* selections) {
  DCHECK_GE(num_total, num_to_select);
  size_t num_left = num_total;
  size_t num_needed = num_to_select;
  for (size_t i = 0; i < num_total && num_needed > 0; ++i) {
    // we have probability = |num_needed| / |num_left| to select
    // this position.
    size_t probability = base::RandInt(0, num_left - 1);
    if (probability < num_needed) {
      selections->push_back(i);
      --num_needed;
    }
    --num_left;
  }
  DCHECK_EQ(num_to_select, selections->size());
}

}  // namespace

namespace autofill {

const size_t PasswordGenerator::kDefaultPasswordLength = 12;

PasswordGenerator::PasswordGenerator(size_t max_length)
    : password_length_(GetLengthFromHint(max_length, kDefaultPasswordLength)) {}
PasswordGenerator::~PasswordGenerator() {}

std::string PasswordGenerator::Generate() const {
  std::string ret;
  CR_DEFINE_STATIC_LOCAL(std::vector<char>, alphanumeric_characters, ());
  if (alphanumeric_characters.empty())
    InitializeAlphaNumericCharacters(&alphanumeric_characters);

  // First, randomly select 4 positions to hold one upper case letter,
  // one lower case letter, one digit, and one other symbol respectively,
  // to make sure at least one of each category of characters will be
  // included in the password.
  std::vector<size_t> positions;
  GetRandomSelection(4u, password_length_, &positions);

  // To enhance the strengh of the password, we random suffle the positions so
  // that the 4 catagories can be put at a random position in it.
  std::random_shuffle(positions.begin(), positions.end());

  // Next, generate each character of the password.
  for (size_t i = 0; i < password_length_; ++i) {
    if (i == positions[0]) {
      // Generate random upper case letter.
      ret.push_back(static_cast<char>(base::RandInt(kMinUpper, kMaxUpper)));
    } else if (i == positions[1]) {
      // Generate random lower case letter.
      ret.push_back(static_cast<char>(base::RandInt(kMinLower, kMaxLower)));
    } else if (i == positions[2]) {
      // Generate random digit.
      ret.push_back(static_cast<char>(base::RandInt(kMinDigit, kMaxDigit)));
    } else if (i == positions[3]) {
      // Generate random other symbol.
      ret.push_back(
          kOtherSymbols[base::RandInt(0, arraysize(kOtherSymbols) - 1)]);
    } else {
      // Generate random alphanumeric character. We don't use other symbols
      // here as most sites don't allow a lot of non-alphanumeric characters.
      ret.push_back(
          alphanumeric_characters.at(
              base::RandInt(0, alphanumeric_characters.size() - 1)));
    }
  }
  return ret;
}

}  // namespace autofill
