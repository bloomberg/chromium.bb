// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This API is a usability layer for direct registry access via NTDLL.
// It allows for "advapi32-free" registry access, which is especially
// useful for accessing registy from DllMain (holding loader lock),
// or if a dependency on/linkage of ADVAPI32.dll is not desired.

// The implementation of this API should only use ntdll and kernel32 system
// DLLs.

// Note that this API is currently lazy initialized.  Any function that is
// NOT merely a wrapper function (i.e. any function that directly interacts with
// NTDLL) will immediately check:
//  if (!g_initialized)
//    InitNativeRegApi();
// There is currently no multi-threading lock around the lazy initialization,
// as the main client for this API (chrome_elf) does not introduce
// a multi-threading concern.  This can easily be changed if needed.

#ifndef CHROME_ELF_NT_REGISTRY_NT_REGISTRY_H_
#define CHROME_ELF_NT_REGISTRY_NT_REGISTRY_H_

#include <vector>

#include "sandbox/win/src/nt_internals.h"  // NTSTATUS

namespace nt {

// These globals are only used in test suites that use reg redirection
// of HKLM and/or HKCU.
extern const size_t g_kRegMaxPathLen;
extern wchar_t HKLM_override[];
extern wchar_t HKCU_override[];

// AUTO will choose depending on system install or not.
// Use HKLM or HKCU to override.
typedef enum _ROOT_KEY { AUTO = 0, HKLM, HKCU } ROOT_KEY;

// Create and/or open a registry key.
// - This function will recursively create multiple sub-keys if required for
//   |key_path|.
// - If the key doesn't need to be left open, pass in nullptr for |out_handle|.
// - This function will happily succeed if the key already exists.
// - Optional |out_handle|.  If nullptr, function will close handle when done.
//   Otherwise, will hold the open handle to the deepest subkey.
// - Caller must call CloseRegKey on returned handle (on success).
bool CreateRegKey(ROOT_KEY root,
                  const wchar_t* key_path,
                  ACCESS_MASK access,
                  HANDLE* out_handle OPTIONAL);

// Open existing registry key.
// - Caller must call CloseRegKey on returned handle (on success).
// - Optional error code can be returned on failure for extra detail.
bool OpenRegKey(ROOT_KEY root,
                const wchar_t* key_path,
                ACCESS_MASK access,
                HANDLE* out_handle,
                NTSTATUS* error_code OPTIONAL);

// Delete a registry key.
// - Caller must still call CloseRegKey after the delete.
bool DeleteRegKey(HANDLE key);

// Delete a registry key.
// - WRAPPER: Function opens and closes the target key for caller.
bool DeleteRegKey(ROOT_KEY root, const wchar_t* key_path);

// Close a registry key handle that was opened with CreateRegKey or OpenRegKey.
void CloseRegKey(HANDLE key);

//------------------------------------------------------------------------------
// Getter functions
//------------------------------------------------------------------------------

// Main function to query a registry value.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Types defined in winnt.h.  E.g.: REG_DWORD, REG_SZ.
// - Caller is responsible for calling "delete[] *out_buffer" (on success).
bool QueryRegKeyValue(HANDLE key,
                      const wchar_t* value_name,
                      ULONG* out_type,
                      BYTE** out_buffer,
                      DWORD* out_size);

// Query DWORD value.
// - WRAPPER: Function works with DWORD data type.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Handle will be left open.  Caller must still call CloseRegKey when done.
bool QueryRegValueDWORD(HANDLE key,
                        const wchar_t* value_name,
                        DWORD* out_dword);

// Query DWORD value.
// - WRAPPER: Function opens and closes the target key for caller, and works
// with DWORD data type.
bool QueryRegValueDWORD(ROOT_KEY root,
                        const wchar_t* key_path,
                        const wchar_t* value_name,
                        DWORD* out_dword);

// Query SZ (string) value.
// - WRAPPER: Function works with SZ data type.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Handle will be left open.  Caller must still call CloseRegKey when done.
bool QueryRegValueSZ(HANDLE key,
                     const wchar_t* value_name,
                     std::wstring* out_sz);

// Query SZ (string) value.
// - WRAPPER: Function opens and closes the target key for caller, and works
// with SZ data type.
bool QueryRegValueSZ(ROOT_KEY root,
                     const wchar_t* key_path,
                     const wchar_t* value_name,
                     std::wstring* out_sz);

// Query MULTI_SZ (multiple strings) value.
// - WRAPPER: Function works with MULTI_SZ data type.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Handle will be left open.  Caller must still call CloseRegKey when done.
bool QueryRegValueMULTISZ(HANDLE key,
                          const wchar_t* value_name,
                          std::vector<std::wstring>* out_multi_sz);

// Query MULTI_SZ (multiple strings) value.
// - WRAPPER: Function opens and closes the target key for caller, and works
// with MULTI_SZ data type.
bool QueryRegValueMULTISZ(ROOT_KEY root,
                          const wchar_t* key_path,
                          const wchar_t* value_name,
                          std::vector<std::wstring>* out_multi_sz);

//------------------------------------------------------------------------------
// Setter functions
//------------------------------------------------------------------------------

// Main function to set a registry value.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Types defined in winnt.h.  E.g.: REG_DWORD, REG_SZ.
bool SetRegKeyValue(HANDLE key,
                    const wchar_t* value_name,
                    ULONG type,
                    const BYTE* data,
                    DWORD data_size);

// Set DWORD value.
// - WRAPPER: Function works with DWORD data type.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Handle will be left open.  Caller must still call CloseRegKey when done.
bool SetRegValueDWORD(HANDLE key, const wchar_t* value_name, DWORD value);

// Set DWORD value.
// - WRAPPER: Function opens and closes the target key for caller, and works
// with DWORD data type.
bool SetRegValueDWORD(ROOT_KEY root,
                      const wchar_t* key_path,
                      const wchar_t* value_name,
                      DWORD value);

// Set SZ (string) value.
// - WRAPPER: Function works with SZ data type.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Handle will be left open.  Caller must still call CloseRegKey when done.
bool SetRegValueSZ(HANDLE key,
                   const wchar_t* value_name,
                   const std::wstring& value);

// Set SZ (string) value.
// - WRAPPER: Function opens and closes the target key for caller, and works
// with SZ data type.
bool SetRegValueSZ(ROOT_KEY root,
                   const wchar_t* key_path,
                   const wchar_t* value_name,
                   const std::wstring& value);

// Set MULTI_SZ (multiple strings) value.
// - WRAPPER: Function works with MULTI_SZ data type.
// - Key handle should have been opened with CreateRegKey or OpenRegKey.
// - Handle will be left open.  Caller must still call CloseRegKey when done.
bool SetRegValueMULTISZ(HANDLE key,
                        const wchar_t* value_name,
                        const std::vector<std::wstring>& values);

// Set MULTI_SZ (multiple strings) value.
// - WRAPPER: Function opens and closes the target key for caller, and works
// with MULTI_SZ data type.
bool SetRegValueMULTISZ(ROOT_KEY root,
                        const wchar_t* key_path,
                        const wchar_t* value_name,
                        const std::vector<std::wstring>& values);

//------------------------------------------------------------------------------
// Utils
//------------------------------------------------------------------------------

// Returns the current user SID in string form.
const wchar_t* GetCurrentUserSidString();

};  // namespace nt

#endif  // CHROME_ELF_NT_REGISTRY_NT_REGISTRY_H_
