// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/import/password_csv_reader.h"

namespace password_manager {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<autofill::PasswordForm> passwords;
  PasswordCSVReader reader;
  reader.DeserializePasswords(
      std::string(reinterpret_cast<const char*>(data), size), &passwords);
  return 0;
}

}  // namespace password_manager
