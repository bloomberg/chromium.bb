// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PASSWORD_GENERATOR_H_
#define CHROME_BROWSER_AUTOFILL_PASSWORD_GENERATOR_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace autofill {

// Class to generate random passwords. Currently we just use a generic algorithm
// for all sites, but eventually we can incorporate additional information to
// determine passwords that are likely to be accepted (i.e. use pattern field,
// previous generated passwords, crowdsourcing, etc.)
class PasswordGenerator {
 public:
  PasswordGenerator();
  ~PasswordGenerator();

  // Returns a random password. The string is guaranteed to be printable and
  // will not include whitespace characters.
  std::string Generate();

 private:

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerator);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_PASSWORD_GENERATOR_H_
