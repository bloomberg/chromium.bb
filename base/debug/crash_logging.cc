// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/crash_logging.h"

#include <map>

#include "base/debug/stack_trace.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace base {
namespace debug {

namespace {

// String used to format chunked key names.
const char kChunkFormatString[] = "%s-%" PRIuS;

// The longest max_length allowed by the system.
const size_t kLargestValueAllowed = 1024;

// The CrashKeyRegistry is a singleton that holds the global state of the crash
// key logging system.
class CrashKeyRegistry {
 public:
  static CrashKeyRegistry* GetInstance() {
    return Singleton<CrashKeyRegistry>::get();
  }

  // Initializes the set of allowed crash keys.
  size_t Init(const CrashKey* const keys, size_t count,
              size_t chunk_max_length) {
    DCHECK(!initialized_) << "Crash logging may only be initialized once";

    chunk_max_length_ = chunk_max_length;

    size_t total_keys = 0;
    for (size_t i = 0; i < count; ++i) {
      crash_keys_.insert(std::make_pair(keys[i].key_name, keys[i]));
      total_keys += NumChunksForLength(keys[i].max_length);
      DCHECK_LT(keys[i].max_length, kLargestValueAllowed);
    }

    DCHECK_EQ(count, crash_keys_.size())
        << "Duplicate crash keys were registered";
    initialized_ = true;

    return total_keys;
  }

  // Sets the function pointers to integrate with the platform-specific crash
  // reporting system.
  void SetFunctions(SetCrashKeyValueFuncT set_key_func,
                    ClearCrashKeyValueFuncT clear_key_func) {
    set_key_func_ = set_key_func;
    clear_key_func_ = clear_key_func;
  }

  // Looks up the CrashKey object by key.
  const CrashKey* LookupCrashKey(const base::StringPiece& key) {
    CrashKeyMap::const_iterator it = crash_keys_.find(key.as_string());
    if (it == crash_keys_.end())
      return nullptr;
    return &(it->second);
  }

  // For a given |length|, computes the number of chunks a value of that size
  // will occupy.
  size_t NumChunksForLength(size_t length) const {
    // Compute (length / g_chunk_max_length_), rounded up.
    return (length + chunk_max_length_ - 1) / chunk_max_length_;
  }

  void SetKeyValue(const base::StringPiece& key,
                   const base::StringPiece& value) {
    set_key_func_(key, value);
  }

  void ClearKey(const base::StringPiece& key) {
    clear_key_func_(key);
  }

  bool is_initialized() const { return initialized_ && set_key_func_; }

  size_t chunk_max_length() const { return chunk_max_length_; }

 private:
  friend struct DefaultSingletonTraits<CrashKeyRegistry>;

  CrashKeyRegistry()
      : initialized_(false),
        chunk_max_length_(0),
        set_key_func_(nullptr),
        clear_key_func_(nullptr) {
  }

  ~CrashKeyRegistry() {}

  bool initialized_;

  // Map of crash key names to registration entries.
  using CrashKeyMap = std::map<base::StringPiece, CrashKey>;
  CrashKeyMap crash_keys_;

  size_t chunk_max_length_;

  // The functions that are called to actually set the key-value pairs in the
  // crash reportng system.
  SetCrashKeyValueFuncT set_key_func_;
  ClearCrashKeyValueFuncT clear_key_func_;

  DISALLOW_COPY_AND_ASSIGN(CrashKeyRegistry);
};

}  // namespace

void SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value) {
  CrashKeyRegistry* registry = CrashKeyRegistry::GetInstance();
  if (!registry->is_initialized())
    return;

  const CrashKey* crash_key = registry->LookupCrashKey(key);

  DCHECK(crash_key) << "All crash keys must be registered before use "
                    << "(key = " << key << ")";

  // Handle the un-chunked case.
  if (!crash_key || crash_key->max_length <= registry->chunk_max_length()) {
    registry->SetKeyValue(key, value);
    return;
  }

  // Unset the unused chunks.
  std::vector<std::string> chunks =
      ChunkCrashKeyValue(*crash_key, value, registry->chunk_max_length());
  for (size_t i = chunks.size();
       i < registry->NumChunksForLength(crash_key->max_length);
       ++i) {
    registry->ClearKey(base::StringPrintf(kChunkFormatString, key.data(), i+1));
  }

  // Set the chunked keys.
  for (size_t i = 0; i < chunks.size(); ++i) {
    registry->SetKeyValue(
        base::StringPrintf(kChunkFormatString, key.data(), i+1),
        chunks[i]);
  }
}

void ClearCrashKey(const base::StringPiece& key) {
  CrashKeyRegistry* registry = CrashKeyRegistry::GetInstance();
  if (!registry->is_initialized())
    return;

  const CrashKey* crash_key = registry->LookupCrashKey(key);

  // Handle the un-chunked case.
  if (!crash_key || crash_key->max_length <= registry->chunk_max_length()) {
    registry->ClearKey(key);
    return;
  }

  for (size_t i = 0;
       i < registry->NumChunksForLength(crash_key->max_length);
       ++i) {
    registry->ClearKey(base::StringPrintf(kChunkFormatString, key.data(), i+1));
  }
}

void SetCrashKeyToStackTrace(const base::StringPiece& key,
                             const StackTrace& trace) {
  size_t count = 0;
  const void* const* addresses = trace.Addresses(&count);
  SetCrashKeyFromAddresses(key, addresses, count);
}

void SetCrashKeyFromAddresses(const base::StringPiece& key,
                              const void* const* addresses,
                              size_t count) {
  std::string value = "<null>";
  if (addresses && count) {
    const size_t kBreakpadValueMax = 255;

    std::vector<std::string> hex_backtrace;
    size_t length = 0;

    for (size_t i = 0; i < count; ++i) {
      std::string s = base::StringPrintf("%p", addresses[i]);
      length += s.length() + 1;
      if (length > kBreakpadValueMax)
        break;
      hex_backtrace.push_back(s);
    }

    value = base::JoinString(hex_backtrace, " ");

    // Warn if this exceeds the breakpad limits.
    DCHECK_LE(value.length(), kBreakpadValueMax);
  }

  SetCrashKeyValue(key, value);
}

ScopedCrashKey::ScopedCrashKey(const base::StringPiece& key,
                               const base::StringPiece& value)
    : key_(key.as_string()) {
  SetCrashKeyValue(key, value);
}

ScopedCrashKey::~ScopedCrashKey() {
  ClearCrashKey(key_);
}

size_t InitCrashKeys(const CrashKey* const keys, size_t count,
                     size_t chunk_max_length) {
  return CrashKeyRegistry::GetInstance()->Init(keys, count, chunk_max_length);
}

const CrashKey* LookupCrashKey(const base::StringPiece& key) {
  return CrashKeyRegistry::GetInstance()->LookupCrashKey(key);
}

void SetCrashKeyReportingFunctions(
    SetCrashKeyValueFuncT set_key_func,
    ClearCrashKeyValueFuncT clear_key_func) {
  CrashKeyRegistry::GetInstance()->SetFunctions(set_key_func, clear_key_func);
}

std::vector<std::string> ChunkCrashKeyValue(const CrashKey& crash_key,
                                            const base::StringPiece& value,
                                            size_t chunk_max_length) {
  std::string value_string = value.substr(0, crash_key.max_length).as_string();
  std::vector<std::string> chunks;
  for (size_t offset = 0; offset < value_string.length(); ) {
    std::string chunk = value_string.substr(offset, chunk_max_length);
    chunks.push_back(chunk);
    offset += chunk.length();
  }
  return chunks;
}

}  // namespace debug
}  // namespace base
