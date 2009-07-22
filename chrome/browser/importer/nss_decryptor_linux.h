// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_LINUX_H_
#define CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_LINUX_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace webkit_glue {
struct PasswordForm;
}

// A wrapper for Firefox NSS decrypt component.
class NSSDecryptor {
 public:
  NSSDecryptor();
  ~NSSDecryptor();

  // Initializes NSS if it hasn't already been initialized.
  bool Init(const std::wstring& /* dll_path */,
            const std::wstring& /* db_path */);

  // Decrypts Firefox stored passwords. Before using this method,
  // make sure Init() returns true.
  std::wstring Decrypt(const std::string& crypt) const;

  // Parses the Firefox password file content, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  void ParseSignons(const std::string& content,
                    std::vector<webkit_glue::PasswordForm>* forms);

 private:
  bool is_nss_initialized_;

  DISALLOW_COPY_AND_ASSIGN(NSSDecryptor);
};

#endif  // CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_LINUX_H_
