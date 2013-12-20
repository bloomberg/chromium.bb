// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_NULL_H_
#define CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_NULL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
}

namespace base {
class FilePath;
}

// A NULL wrapper for Firefox NSS decrypt component, for use in builds where
// we do not have the NSS library.
class NSSDecryptor {
 public:
  NSSDecryptor() {}
  bool Init(const base::FilePath& dll_path, const base::FilePath& db_path) {
    return false;
  }
  base::string16 Decrypt(const std::string& crypt) const {
    return base::string16();
  }
  void ParseSignons(const std::string& content,
                    std::vector<autofill::PasswordForm>* forms) {}
  bool ReadAndParseSignons(const base::FilePath& sqlite_file,
                           std::vector<autofill::PasswordForm>* forms) {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NSSDecryptor);
};

#endif  // CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_NULL_H_
