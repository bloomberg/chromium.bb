// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/blacklist/blacklist.h"

#include <windows.h>

#include <assert.h>
#include <string.h>

#include "chrome/chrome_elf/third_party_dlls/hardcoded_blocklist.h"

namespace blacklist {

namespace {

// Record if the blacklist was successfully initialized so processes can easily
// determine if the blacklist is enabled for them.
bool g_blacklist_initialized = false;

}  // namespace

using third_party_dlls::g_troublesome_dlls;
using third_party_dlls::kTroublesomeDllsMaxCount;

bool g_blocked_dlls[kTroublesomeDllsMaxCount] = {};
int g_num_blocked_dlls = 0;

int BlacklistSize() {
  int size = -1;
  while (blacklist::g_troublesome_dlls[++size] != NULL) {
  }

  return size;
}

bool IsBlacklistInitialized() {
  return g_blacklist_initialized;
}

bool AddDllToBlacklist(const wchar_t* dll_name) {
  int blacklist_size = BlacklistSize();
  // We need to leave one space at the end for the null pointer.
  if (blacklist_size + 1 >= static_cast<int>(kTroublesomeDllsMaxCount))
    return false;
  for (int i = 0; i < blacklist_size; ++i) {
    if (!_wcsicmp(g_troublesome_dlls[i], dll_name))
      return true;
  }

  // Copy string to blacklist.
  wchar_t* str_buffer = new wchar_t[wcslen(dll_name) + 1];
  wcscpy(str_buffer, dll_name);

  g_troublesome_dlls[blacklist_size] = str_buffer;
  g_blocked_dlls[blacklist_size] = false;
  return true;
}

bool RemoveDllFromBlacklist(const wchar_t* dll_name) {
  int blacklist_size = BlacklistSize();
  for (int i = 0; i < blacklist_size; ++i) {
    if (!_wcsicmp(g_troublesome_dlls[i], dll_name)) {
      // Found the thing to remove. Delete it then replace it with the last
      // element.
      delete[] g_troublesome_dlls[i];
      g_troublesome_dlls[i] = g_troublesome_dlls[blacklist_size - 1];
      g_troublesome_dlls[blacklist_size - 1] = NULL;

      // Also update the stats recording if we have blocked this dll or not.
      if (g_blocked_dlls[i])
        --g_num_blocked_dlls;
      g_blocked_dlls[i] = g_blocked_dlls[blacklist_size - 1];
      return true;
    }
  }
  return false;
}

// TODO(csharp): Maybe store these values in the registry so we can
// still report them if Chrome crashes early.
void SuccessfullyBlocked(const wchar_t** blocked_dlls, int* size) {
  if (size == NULL)
    return;

  // If the array isn't valid or big enough, just report the size it needs to
  // be and return.
  if (blocked_dlls == NULL && *size < g_num_blocked_dlls) {
    *size = g_num_blocked_dlls;
    return;
  }

  *size = g_num_blocked_dlls;

  int strings_to_fill = 0;
  for (int i = 0; strings_to_fill < g_num_blocked_dlls && g_troublesome_dlls[i];
       ++i) {
    if (g_blocked_dlls[i]) {
      blocked_dlls[strings_to_fill] = g_troublesome_dlls[i];
      ++strings_to_fill;
    }
  }
}

void BlockedDll(size_t blocked_index) {
  assert(blocked_index < kTroublesomeDllsMaxCount);

  if (!g_blocked_dlls[blocked_index] &&
      blocked_index < kTroublesomeDllsMaxCount) {
    ++g_num_blocked_dlls;
    g_blocked_dlls[blocked_index] = true;
  }
}

int DllMatch(const std::wstring& module_name) {
  if (module_name.empty())
    return -1;

  for (int i = 0; blacklist::g_troublesome_dlls[i] != NULL; ++i) {
    if (_wcsicmp(module_name.c_str(), blacklist::g_troublesome_dlls[i]) == 0)
      return i;
  }
  return -1;
}

}  // namespace blacklist
