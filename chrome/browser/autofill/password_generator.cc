// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/password_generator.h"

#include "base/rand_util.h"

const int kMinChar = 33;   // First printable character '!'
const int kMaxChar = 126;  // Last printable character '~'
const int kPasswordLength = 12;

namespace autofill {

PasswordGenerator::PasswordGenerator() {}
PasswordGenerator::~PasswordGenerator() {}

std::string PasswordGenerator::Generate() {
  std::string ret;
  for (int i = 0; i < kPasswordLength; i++) {
    ret.push_back(static_cast<char>(base::RandInt(kMinChar, kMaxChar)));
  }
  return ret;
}

}  // namespace autofill
