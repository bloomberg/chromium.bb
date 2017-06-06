// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_util.h"

#include <windows.h>
#include <winternl.h>

namespace base {
namespace win {

#if DCHECK_IS_ON()

namespace {

enum class ComInitStatus {
  NONE,
  STA,
  MTA,
};

// Derived from combase.dll.
struct OleTlsData {
  enum ApartmentFlags {
    STA = 0x80,
    MTA = 0x140,
  };

  void* thread_base;
  void* sm_allocator;
  DWORD apartment_id;
  DWORD apartment_flags;
  // There are many more fields than this, but for our purposes, we only care
  // about |apartment_flags|. Correctly declaring the previous types allows this
  // to work between x86 and x64 builds.
};

ComInitStatus GetComInitStatusForThread() {
  TEB* teb = NtCurrentTeb();
  OleTlsData* ole_tls_data = reinterpret_cast<OleTlsData*>(teb->ReservedForOle);
  if (!ole_tls_data)
    return ComInitStatus::NONE;

  if (ole_tls_data->apartment_flags & OleTlsData::ApartmentFlags::STA)
    return ComInitStatus::STA;

  if ((ole_tls_data->apartment_flags & OleTlsData::ApartmentFlags::MTA) ==
      OleTlsData::ApartmentFlags::MTA) {
    return ComInitStatus::MTA;
  }

  return ComInitStatus::NONE;
}

}  // namespace

void AssertComInitialized() {
  DCHECK_NE(ComInitStatus::NONE, GetComInitStatusForThread());
}

#endif  // DCHECK_IS_ON()

}  // namespace win
}  // namespace base
