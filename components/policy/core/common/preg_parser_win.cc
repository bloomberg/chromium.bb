// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/preg_parser_win.h"

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/registry_dict_win.h"

namespace policy {
namespace preg_parser {

const char kPRegFileHeader[8] =
    { 'P', 'R', 'e', 'g', '\x01', '\x00', '\x00', '\x00' };

// Maximum PReg file size we're willing to accept.
const int64_t kMaxPRegFileSize = 1024 * 1024 * 16;

// Constants for PReg file delimiters.
const base::char16 kDelimBracketOpen = L'[';
const base::char16 kDelimBracketClose = L']';
const base::char16 kDelimSemicolon = L';';

// Registry path separator.
const base::char16 kRegistryPathSeparator[] = L"\\";

// Magic strings for the PReg value field to trigger special actions.
const char kActionTriggerPrefix[] = "**";
const char kActionTriggerDeleteValues[] = "deletevalues";
const char kActionTriggerDel[] = "del.";
const char kActionTriggerDelVals[] = "delvals";
const char kActionTriggerDeleteKeys[] = "deletekeys";
const char kActionTriggerSecureKey[] = "securekey";
const char kActionTriggerSoft[] = "soft";

// Returns the character at |cursor| and increments it, unless the end is here
// in which case -1 is returned.
int NextChar(const uint8_t** cursor, const uint8_t* end) {
  // Only read the character if a full base::char16 is available.
  if (*cursor + sizeof(base::char16) > end)
    return -1;

  int result = **cursor | (*(*cursor + 1) << 8);
  *cursor += sizeof(base::char16);
  return result;
}

// Reads a fixed-size field from a PReg file.
bool ReadFieldBinary(const uint8_t** cursor,
                     const uint8_t* end,
                     uint32_t size,
                     uint8_t* data) {
  if (size == 0)
    return true;

  const uint8_t* field_end = *cursor + size;
  if (field_end <= *cursor || field_end > end)
    return false;
  std::copy(*cursor, field_end, data);
  *cursor = field_end;
  return true;
}

bool ReadField32(const uint8_t** cursor, const uint8_t* end, uint32_t* data) {
  uint32_t value = 0;
  if (!ReadFieldBinary(cursor, end, sizeof(uint32_t),
                       reinterpret_cast<uint8_t*>(&value))) {
    return false;
  }
  *data = base::ByteSwapToLE32(value);
  return true;
}

// Reads a string field from a file.
bool ReadFieldString(const uint8_t** cursor,
                     const uint8_t* end,
                     base::string16* str) {
  int current = -1;
  while ((current = NextChar(cursor, end)) > 0x0000)
    *str += current;

  return current == L'\0';
}

std::string DecodePRegStringValue(const std::vector<uint8_t>& data) {
  size_t len = data.size() / sizeof(base::char16);
  if (len <= 0)
    return std::string();

  const base::char16* chars =
      reinterpret_cast<const base::char16*>(data.data());
  base::string16 result;
  std::transform(chars, chars + len - 1, std::back_inserter(result),
                 std::ptr_fun(base::ByteSwapToLE16));
  return base::UTF16ToUTF8(result);
}

// Decodes a value from a PReg file given as a uint8_t vector.
bool DecodePRegValue(uint32_t type,
                     const std::vector<uint8_t>& data,
                     std::unique_ptr<base::Value>* value) {
  switch (type) {
    case REG_SZ:
    case REG_EXPAND_SZ:
      value->reset(new base::StringValue(DecodePRegStringValue(data)));
      return true;
    case REG_DWORD_LITTLE_ENDIAN:
    case REG_DWORD_BIG_ENDIAN:
      if (data.size() == sizeof(uint32_t)) {
        uint32_t val = *reinterpret_cast<const uint32_t*>(data.data());
        if (type == REG_DWORD_BIG_ENDIAN)
          val = base::NetToHost32(val);
        else
          val = base::ByteSwapToLE32(val);
        value->reset(new base::FundamentalValue(static_cast<int>(val)));
        return true;
      } else {
        LOG(ERROR) << "Bad data size " << data.size();
      }
      break;
    case REG_NONE:
    case REG_LINK:
    case REG_MULTI_SZ:
    case REG_RESOURCE_LIST:
    case REG_FULL_RESOURCE_DESCRIPTOR:
    case REG_RESOURCE_REQUIREMENTS_LIST:
    case REG_QWORD_LITTLE_ENDIAN:
    default:
      LOG(ERROR) << "Unsupported registry data type " << type;
  }

  return false;
}

// Adds the record data passed via parameters to |dict| in case the data is
// relevant policy for Chromium.
void HandleRecord(const base::string16& key_name,
                  const base::string16& value,
                  uint32_t type,
                  const std::vector<uint8_t>& data,
                  RegistryDict* dict) {
  // Locate/create the dictionary to place the value in.
  std::vector<base::string16> path;

  for (const base::string16& entry :
       base::SplitString(key_name, kRegistryPathSeparator,
                         base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (entry.empty())
      continue;
    const std::string name = base::UTF16ToUTF8(entry);
    RegistryDict* subdict = dict->GetKey(name);
    if (!subdict) {
      subdict = new RegistryDict();
      dict->SetKey(name, base::WrapUnique(subdict));
    }
    dict = subdict;
  }

  if (value.empty())
    return;

  std::string value_name(base::UTF16ToUTF8(value));
  if (!base::StartsWith(value_name, kActionTriggerPrefix,
                        base::CompareCase::SENSITIVE)) {
    std::unique_ptr<base::Value> value;
    if (DecodePRegValue(type, data, &value))
      dict->SetValue(value_name, std::move(value));
    return;
  }

  std::string action_trigger(base::ToLowerASCII(value_name.substr(
      arraysize(kActionTriggerPrefix) - 1)));
  if (action_trigger == kActionTriggerDeleteValues) {
    for (const std::string& value :
         base::SplitString(DecodePRegStringValue(data), ";",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY))
      dict->RemoveValue(value);
  } else if (base::StartsWith(action_trigger, kActionTriggerDeleteKeys,
                              base::CompareCase::SENSITIVE)) {
    for (const std::string& key :
         base::SplitString(DecodePRegStringValue(data), ";",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY))
      dict->RemoveKey(key);
  } else if (base::StartsWith(action_trigger, kActionTriggerDel,
                              base::CompareCase::SENSITIVE)) {
    dict->RemoveValue(
        value_name.substr(arraysize(kActionTriggerPrefix) - 1 +
                          arraysize(kActionTriggerDel) - 1));
  } else if (base::StartsWith(action_trigger, kActionTriggerDelVals,
                              base::CompareCase::SENSITIVE)) {
    // Delete all values.
    dict->ClearValues();
  } else if (base::StartsWith(action_trigger, kActionTriggerSecureKey,
                              base::CompareCase::SENSITIVE) ||
             base::StartsWith(action_trigger, kActionTriggerSoft,
                              base::CompareCase::SENSITIVE)) {
    // Doesn't affect values.
  } else {
    LOG(ERROR) << "Bad action trigger " << value_name;
  }
}

bool ReadFile(const base::FilePath& file_path,
              const base::string16& root,
              RegistryDict* dict,
              PolicyLoadStatusSample* status) {
  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(file_path) || !mapped_file.IsValid()) {
    PLOG(ERROR) << "Failed to map " << file_path.value();
    status->Add(POLICY_LOAD_STATUS_READ_ERROR);
    return false;
  }

  if (mapped_file.length() > kMaxPRegFileSize) {
    LOG(ERROR) << "PReg file " << file_path.value() << " too large: "
               << mapped_file.length();
    status->Add(POLICY_LOAD_STATUS_TOO_BIG);
    return false;
  }

  // Check the header.
  const int kHeaderSize = arraysize(kPRegFileHeader);
  if (mapped_file.length() < kHeaderSize ||
      memcmp(kPRegFileHeader, mapped_file.data(), kHeaderSize) != 0) {
    LOG(ERROR) << "Bad policy file " << file_path.value();
    status->Add(POLICY_LOAD_STATUS_PARSE_ERROR);
    return false;
  }

  // Parse file contents, which is UCS-2 and little-endian. The latter I
  // couldn't find documentation on, but the example I saw were all
  // little-endian. It'd be interesting to check on big-endian hardware.
  const uint8_t* cursor = mapped_file.data() + kHeaderSize;
  const uint8_t* end = mapped_file.data() + mapped_file.length();
  while (true) {
    if (cursor == end)
      return true;

    if (NextChar(&cursor, end) != kDelimBracketOpen)
      break;

    // Read the record fields.
    base::string16 key_name;
    base::string16 value;
    uint32_t type = 0;
    uint32_t size = 0;
    std::vector<uint8_t> data;

    if (!ReadFieldString(&cursor, end, &key_name))
      break;

    int current = NextChar(&cursor, end);
    if (current == kDelimSemicolon) {
      if (!ReadFieldString(&cursor, end, &value))
        break;
      current = NextChar(&cursor, end);
    }

    if (current == kDelimSemicolon) {
      if (!ReadField32(&cursor, end, &type))
        break;
      current = NextChar(&cursor, end);
    }

    if (current == kDelimSemicolon) {
      if (!ReadField32(&cursor, end, &size))
        break;
      current = NextChar(&cursor, end);
    }

    if (current == kDelimSemicolon) {
      if (size > kMaxPRegFileSize)
        break;
      data.resize(size);
      if (!ReadFieldBinary(&cursor, end, size, data.data()))
        break;
      current = NextChar(&cursor, end);
    }

    if (current != kDelimBracketClose)
      break;

    // Process the record if it is within the |root| subtree.
    if (base::StartsWith(base::i18n::ToLower(key_name),
                         base::i18n::ToLower(root),
                         base::CompareCase::SENSITIVE))
      HandleRecord(key_name.substr(root.size()), value, type, data, dict);
  }

  LOG(ERROR) << "Error parsing " << file_path.value() << " at offset "
             << reinterpret_cast<const uint8_t*>(cursor - 1) -
                    mapped_file.data();
  status->Add(POLICY_LOAD_STATUS_PARSE_ERROR);
  return false;
}

}  // namespace preg_parser
}  // namespace policy
