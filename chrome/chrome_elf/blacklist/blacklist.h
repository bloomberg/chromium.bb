// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_BLACKLIST_BLACKLIST_H_
#define CHROME_CHROME_ELF_BLACKLIST_BLACKLIST_H_

#include <stddef.h>

#include <string>

namespace blacklist {

// Return the size of the current blacklist.
extern "C" int BlacklistSize();

// Returns if true if the blacklist has been initialized.
extern "C" bool IsBlacklistInitialized();

// Adds the given dll name to the blacklist. Returns true if the dll name is in
// the blacklist when this returns, false on error. Note that this will copy
// |dll_name| and will leak it on exit if the string is not subsequently removed
// using RemoveDllFromBlacklist.
// Exposed for testing only, this shouldn't be exported from chrome_elf.dll.
extern "C" bool AddDllToBlacklist(const wchar_t* dll_name);

// Removes the given dll name from the blacklist. Returns true if it was
// removed, false on error.
// Exposed for testing only, this shouldn't be exported from chrome_elf.dll.
extern "C" bool RemoveDllFromBlacklist(const wchar_t* dll_name);

// Returns a list of all the dlls that have been successfully blocked by the
// blacklist via blocked_dlls, if there is enough space (according to |size|).
// |size| will always be modified to be the number of dlls that were blocked.
// The caller doesn't own the strings and isn't expected to free them. These
// strings won't be hanging unless RemoveDllFromBlacklist is called, but it
// is only exposed in tests (and should stay that way).
extern "C" void SuccessfullyBlocked(const wchar_t** blocked_dlls, int* size);

// Record that the dll at the given index was blocked.
extern "C" void BlockedDll(size_t blocked_index);

// Legacy match function.
// Returns the index of the blacklist found in |g_troublesome_dlls|, or -1.
int DllMatch(const std::wstring& module_name);

}  // namespace blacklist

#endif  // CHROME_CHROME_ELF_BLACKLIST_BLACKLIST_H_
