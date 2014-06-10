// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/password_generator.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "third_party/fips181/fips181.h"

const int kMinUpper = 65;  // First upper case letter 'A'
const int kMaxUpper = 90;  // Last upper case letter 'Z'
const int kMinLower = 97;  // First lower case letter 'a'
const int kMaxLower = 122; // Last lower case letter 'z'
const int kMinDigit = 48;  // First digit '0'
const int kMaxDigit = 57;  // Last digit '9'
const int kMinPasswordLength = 4;
const int kMaxPasswordLength = 15;

namespace {

// A helper function to get the length of the generated password from
// |max_length| retrieved from input password field.
int GetLengthFromHint(int max_length, int default_length) {
  if (max_length >= kMinPasswordLength && max_length <= kMaxPasswordLength)
    return max_length;
  else
    return default_length;
}

// We want the password to have uppercase, lowercase, and at least one number.
bool VerifyPassword(const std::string& password) {
  int num_lower_case = 0;
  int num_upper_case = 0;
  int num_digits = 0;

  for (size_t i = 0; i < password.size(); ++i) {
    if (password[i] >= kMinUpper && password[i] <= kMaxUpper)
      ++num_upper_case;
    if (password[i] >= kMinLower && password[i] <= kMaxLower)
      ++num_lower_case;
    if (password[i] >= kMinDigit && password[i] <= kMaxDigit)
      ++num_digits;
  }

  return num_lower_case && num_upper_case && num_digits;
}

// Make sure that there is at least one upper case and one number in the
// password. Assume that there already exists a lower case letter as it's the
// default from gen_pron_pass.
void ForceFixPassword(std::string* password) {
  for (std::string::iterator iter = password->begin();
       iter != password->end(); ++iter) {
    if (islower(*iter)) {
      *iter = base::ToUpperASCII(*iter);
      break;
    }
  }
  for (std::string::reverse_iterator iter = password->rbegin();
       iter != password->rend(); ++iter) {
    if (islower(*iter)) {
      *iter = base::RandInt(kMinDigit, kMaxDigit);
      break;
    }
  }
}

}  // namespace

namespace autofill {

const int PasswordGenerator::kDefaultPasswordLength = 12;

PasswordGenerator::PasswordGenerator(int max_length)
    : password_length_(GetLengthFromHint(max_length, kDefaultPasswordLength)) {}
PasswordGenerator::~PasswordGenerator() {}

std::string PasswordGenerator::Generate() const {
  char password[255];
  char unused_hypenated_password[255];
  // Generate passwords that have numbers and upper and lower case letters.
  // No special characters included for now.
  unsigned int mode = S_NB | S_CL | S_SL;

  // gen_pron_pass() doesn't guarantee that it includes all of the type given
  // in mode, so regenerate a few times if neccessary.
  // TODO(gcasto): Is it worth regenerating at all?
  for (int i = 0; i < 10; ++i) {
    gen_pron_pass(password, unused_hypenated_password,
                  password_length_, password_length_, mode);
    if (VerifyPassword(password))
      break;
  }

  // If the password still isn't conforming after a few iterations, force it
  // to be so. This may change a syllable in the password.
  std::string str_password(password);
  if (!VerifyPassword(str_password)) {
    ForceFixPassword(&str_password);
  }
  return str_password;
}

}  // namespace autofill
