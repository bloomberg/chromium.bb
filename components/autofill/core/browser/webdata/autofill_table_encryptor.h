// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_ENCRYPTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_ENCRYPTOR_H_

#include <string>

#include "base/strings/string16.h"

namespace autofill {
// Encryptor used by Autofill table.
class AutofillTableEncryptor {
 public:
  virtual ~AutofillTableEncryptor() = default;

  virtual bool EncryptString16(const base::string16& plaintext,
                               std::string* ciphertext) const = 0;
  virtual bool DecryptString16(const std::string& ciphertext,
                               base::string16* plaintext) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_ENCRYPTOR_H_
