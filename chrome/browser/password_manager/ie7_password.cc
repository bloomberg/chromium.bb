// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ie7_password.h"

#include <wincrypt.h>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/string_util.h"

namespace {

// Structures that IE7/IE8 use to store a username/password.
// Some of the fields might have been incorrectly reverse engineered.
struct PreHeader {
  DWORD pre_header_size;  // Size of this header structure. Always 12.
  DWORD header_size;      // Size of the real Header: sizeof(Header) +
                          // item_count * sizeof(Entry);
  DWORD data_size;        // Size of the data referenced by the entries.
};

struct Header {
  char wick[4];             // The string "WICK". I don't know what it means.
  DWORD fixed_header_size;  // The size of this structure without the entries:
                            // sizeof(Header).
  DWORD item_count;         // Number of entries. It should always be 2. One for
                            // the username, and one for the password.
  wchar_t two_letters[2];   // Two unknown bytes.
  DWORD unknown[2];         // Two unknown DWORDs.
};

struct Entry {
  DWORD offset;         // Offset where the data referenced by this entry is
                        // located.
  FILETIME time_stamp;  // Timestamp when the password got added.
  DWORD string_length;  // The length of the data string.
};

// Main data structure.
struct PasswordEntry {
  PreHeader pre_header;  // Contains the size of the different sections.
  Header header;         // Contains the number of items.
  Entry entry[1];        // List of entries containing a string. The first one
                         // is the username, the second one if the password.
};

}  // namespace

namespace ie7_password {

bool GetUserPassFromData(const std::vector<unsigned char>& data,
                         std::wstring* username,
                         std::wstring* password) {
  const PasswordEntry* information =
      reinterpret_cast<const PasswordEntry*>(&data.front());

  // Some expected values. If it's not what we expect we don't even try to
  // understand the data.
  if (information->pre_header.pre_header_size != sizeof(PreHeader))
    return false;

  if (information->header.item_count != 2)  // Username and Password
    return false;

  if (information->header.fixed_header_size != sizeof(Header))
    return false;

  const uint8* ptr = &data.front();
  const uint8* offset_to_data = ptr + information->pre_header.header_size +
                                information->pre_header.pre_header_size;

  const Entry* user_entry = information->entry;
  const Entry* pass_entry = user_entry+1;

  *username = reinterpret_cast<const wchar_t*>(offset_to_data +
                                               user_entry->offset);
  *password = reinterpret_cast<const wchar_t*>(offset_to_data +
                                               pass_entry->offset);
  return true;
}

std::wstring GetUrlHash(const std::wstring& url) {
  std::wstring lower_case_url = StringToLowerASCII(url);
  // Get a data buffer out of our std::wstring to pass to SHA1HashString.
  std::string url_buffer(
      reinterpret_cast<const char*>(lower_case_url.c_str()),
      (lower_case_url.size() + 1) * sizeof(wchar_t));
  std::string hash_bin = base::SHA1HashString(url_buffer);

  std::wstring url_hash;

  // Transform the buffer to an hexadecimal string.
  unsigned char checksum = 0;
  for (size_t i = 0; i < hash_bin.size(); ++i) {
    // std::string gives signed chars, which mess with StringPrintf and
    // check_sum.
    unsigned char hash_byte = static_cast<unsigned char>(hash_bin[i]);
    checksum += hash_byte;
    url_hash += StringPrintf(L"%2.2X", static_cast<unsigned>(hash_byte));
  }
  url_hash += StringPrintf(L"%2.2X", checksum);

  return url_hash;
}

bool DecryptPassword(const std::wstring& url,
                     const std::vector<unsigned char>& data,
                     std::wstring* username, std::wstring* password) {
  std::wstring lower_case_url = StringToLowerASCII(url);
  DATA_BLOB input = {0};
  DATA_BLOB output = {0};
  DATA_BLOB url_key = {0};

  input.pbData = const_cast<unsigned char*>(&data.front());
  input.cbData = static_cast<DWORD>((data.size()) *
                                    sizeof(std::string::value_type));

  url_key.pbData = reinterpret_cast<unsigned char*>(
                      const_cast<wchar_t*>(lower_case_url.data()));
  url_key.cbData = static_cast<DWORD>((lower_case_url.size() + 1) *
                                      sizeof(std::wstring::value_type));

  if (CryptUnprotectData(&input, NULL, &url_key, NULL, NULL,
                         CRYPTPROTECT_UI_FORBIDDEN, &output)) {
    // Now that we have the decrypted information, we need to understand it.
    std::vector<unsigned char> decrypted_data;
    decrypted_data.resize(output.cbData);
    memcpy(&decrypted_data.front(), output.pbData, output.cbData);

    GetUserPassFromData(decrypted_data, username, password);

    LocalFree(output.pbData);
    return true;
  }

  return false;
}

}  // namespace ie7_password
