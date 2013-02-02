// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_NULL_H_
#define CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_NULL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

namespace base {
class FilePath;
}

namespace content {
struct PasswordForm;
}

// A NULL wrapper for Firefox NSS decrypt component, for use in builds where
// we do not have the NSS library.
class NSSDecryptor {
 public:
  NSSDecryptor() {}
  bool Init(const base::FilePath& dll_path, const base::FilePath& db_path) { return false; }
  string16 Decrypt(const std::string& crypt) const { return string16(); }
  void ParseSignons(const std::string& content,
                    std::vector<content::PasswordForm>* forms) {}
  bool ReadAndParseSignons(const base::FilePath& sqlite_file,
                           std::vector<content::PasswordForm>* forms) {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NSSDecryptor);
};

#endif  // CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_NULL_H_
