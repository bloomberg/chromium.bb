// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_SYSTEM_NSS_H_
#define CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_SYSTEM_NSS_H_
#pragma once

#include <secmodt.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

class FilePath;

namespace webkit_glue {
struct PasswordForm;
}

// A wrapper for Firefox NSS decrypt component.
class NSSDecryptor {
 public:
  NSSDecryptor();
  ~NSSDecryptor();

  // Initializes NSS if it hasn't already been initialized.
  bool Init(const FilePath& dll_path, const FilePath& db_path);

  // Decrypts Firefox stored passwords. Before using this method,
  // make sure Init() returns true.
  string16 Decrypt(const std::string& crypt) const;

  // Parses the Firefox password file content, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  void ParseSignons(const std::string& content,
                    std::vector<webkit_glue::PasswordForm>* forms);

  // Reads and parses the Firefox password sqlite db, decrypts the
  // username/password and reads other related information.
  // The result will be stored in |forms|.
  bool ReadAndParseSignons(const FilePath& sqlite_file,
                           std::vector<webkit_glue::PasswordForm>* forms);
 private:
  // Does not actually free the slot, since we'll free it when NSSDecryptor is
  // destroyed.
  void FreeSlot(PK11SlotInfo* slot) const {}
  PK11SlotInfo* GetKeySlotForDB() const { return db_slot_; }

  SECStatus PK11SDR_DecryptWithSlot(
      PK11SlotInfo* slot, SECItem* data, SECItem* result, void* cx) const;

  bool is_nss_initialized_;
  PK11SlotInfo* db_slot_;

  DISALLOW_COPY_AND_ASSIGN(NSSDecryptor);
};

#endif  // CHROME_BROWSER_IMPORTER_NSS_DECRYPTOR_SYSTEM_NSS_H_
