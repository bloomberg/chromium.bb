// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"

#include <string>

#include "base/files/file_path.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace chromeos {
namespace smb_client {
namespace {

constexpr char kDelimiter[] = "@@";
constexpr char kKerberosSymbol[] = "kerberos_chromad";
constexpr int kRandomIdBytes = 8;

std::vector<std::string> GetComponents(const std::string& file_system_id) {
  const std::vector<std::string> components =
      SplitString(file_system_id, kDelimiter, base::TRIM_WHITESPACE,
                  base::SPLIT_WANT_NONEMPTY);

  DCHECK_GE(components.size(), 2u);
  DCHECK_LE(components.size(), 3u);

  return components;
}

std::string GenerateRandomId() {
  char rand_bytes[kRandomIdBytes];
  base::RandBytes(rand_bytes, sizeof(rand_bytes));
  // Encoding to hex ensure that there are no non-alpha characters in the id
  // (i.e. no @ delimiters).
  return base::HexEncode(rand_bytes, sizeof(rand_bytes));
}

}  // namespace.

std::string CreateFileSystemId(const base::FilePath& share_path,
                               bool is_kerberos_chromad) {
  const std::string file_system_id =
      base::StrCat({GenerateRandomId(), kDelimiter, share_path.value()});
  if (is_kerberos_chromad) {
    return base::StrCat({file_system_id, kDelimiter, kKerberosSymbol});
  }
  return file_system_id;
}

base::FilePath GetSharePathFromFileSystemId(const std::string& file_system_id) {
  const std::vector<std::string> components = GetComponents(file_system_id);
  DCHECK_GE(components.size(), 1u);

  return base::FilePath(components[1]);
}

bool IsKerberosChromadFileSystemId(const std::string& file_system_id) {
  const std::vector<std::string> components = GetComponents(file_system_id);

  return components.size() >= 3 && components[2] == kKerberosSymbol;
}

}  // namespace smb_client
}  // namespace chromeos
