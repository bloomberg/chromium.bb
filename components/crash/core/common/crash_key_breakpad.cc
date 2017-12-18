// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This file is only compiled when Crashpad is not used as the crash
// reproter.

#include "components/crash/core/common/crash_key.h"

#include "base/format_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key_base_support.h"
#include "components/crash/core/common/crash_key_internal.h"

#if defined(OS_MACOSX) || defined(OS_IOS) || defined(OS_WIN)
#error "This file should not be used when Crashpad is available, nor on iOS."
#endif

namespace crash_reporter {
namespace internal {

namespace {

// String used to format chunked key names. The __1 through __N syntax is
// recognized by the crash collector, which will then stitch the numbered
// parts back into a single string value.
const char kChunkFormatString[] = "%s__%" PRIuS;

static TransitionalCrashKeyStorage* g_storage = nullptr;

}  // namespace

TransitionalCrashKeyStorage* GetCrashKeyStorage() {
  if (!g_storage) {
    g_storage = new internal::TransitionalCrashKeyStorage();
  }
  return g_storage;
}

void ResetCrashKeyStorageForTesting() {
  auto* storage = g_storage;
  g_storage = nullptr;
  delete storage;
}

void CrashKeyStringImpl::Set(base::StringPiece value) {
  const size_t kValueMaxLength = index_array_count_ * kCrashKeyStorageValueSize;

  TransitionalCrashKeyStorage* storage = GetCrashKeyStorage();

  value = value.substr(0, kValueMaxLength);

  // If there is only one slot for the value, then handle it directly.
  if (index_array_count_ == 1) {
    std::string value_string = value.as_string();
    if (is_set()) {
      storage->SetValueAtIndex(index_array_[0], value_string.c_str());
    } else {
      index_array_[0] = storage->SetKeyValue(name_, value_string.c_str());
    }
    return;
  }

  // Otherwise, break the value into chunks labeled name-1 through name-N,
  // where N is |index_array_count_|.
  size_t offset = 0;
  for (size_t i = 0; i < index_array_count_; ++i) {
    if (offset < value.length()) {
      // The storage NUL-terminates the value, so ensure that a byte is
      // not lost when setting individaul chunks.
      base::StringPiece chunk =
          value.substr(offset, kCrashKeyStorageValueSize - 1);
      offset += chunk.length();

      if (index_array_[i] == TransitionalCrashKeyStorage::num_entries) {
        std::string chunk_name =
            base::StringPrintf(kChunkFormatString, name_, i + 1);
        index_array_[i] =
            storage->SetKeyValue(chunk_name.c_str(), chunk.data());
      } else {
        storage->SetValueAtIndex(index_array_[i], chunk.data());
      }
    } else {
      storage->RemoveAtIndex(index_array_[i]);
      index_array_[i] = TransitionalCrashKeyStorage::num_entries;
    }
  }
}

void CrashKeyStringImpl::Clear() {
  for (size_t i = 0; i < index_array_count_; ++i) {
    GetCrashKeyStorage()->RemoveAtIndex(index_array_[i]);
    index_array_[i] = TransitionalCrashKeyStorage::num_entries;
  }
}

bool CrashKeyStringImpl::is_set() const {
  return index_array_[0] != TransitionalCrashKeyStorage::num_entries;
}

}  // namespace internal

void InitializeCrashKeys() {
  internal::GetCrashKeyStorage();
  InitializeCrashKeyBaseSupport();
}

}  // namespace crash_reporter
