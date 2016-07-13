// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/nt_registry/nt_registry.h"

namespace {

// Function pointers used for registry access.
RtlInitUnicodeStringFunction g_rtl_init_unicode_string = nullptr;
NtCreateKeyFunction g_nt_create_key = nullptr;
NtDeleteKeyFunction g_nt_delete_key = nullptr;
NtOpenKeyExFunction g_nt_open_key_ex = nullptr;
NtCloseFunction g_nt_close = nullptr;
NtQueryValueKeyFunction g_nt_query_value_key = nullptr;
NtSetValueKeyFunction g_nt_set_value_key = nullptr;

// Lazy init.  No concern about concurrency in chrome_elf.
bool g_initialized = false;
bool g_system_install = false;
bool g_reg_redirection = false;
const size_t g_kMaxPathLen = 255;
wchar_t g_kRegPathHKLM[] = L"\\Registry\\Machine\\";
wchar_t g_kRegPathHKCU[g_kMaxPathLen] = L"";
wchar_t g_current_user_sid_string[g_kMaxPathLen] = L"";
wchar_t g_override_path[g_kMaxPathLen] = L"";

// Not using install_util, to prevent circular dependency.
bool IsThisProcSystem() {
  wchar_t program_dir[MAX_PATH] = {};
  wchar_t* cmd_line = GetCommandLineW();
  // If our command line starts with the "Program Files" or
  // "Program Files (x86)" path, we're system.
  DWORD ret = ::GetEnvironmentVariable(L"PROGRAMFILES", program_dir, MAX_PATH);
  if (ret && ret < MAX_PATH && !::wcsncmp(cmd_line, program_dir, ret))
    return true;

  ret = ::GetEnvironmentVariable(L"PROGRAMFILES(X86)", program_dir, MAX_PATH);
  if (ret && ret < MAX_PATH && !::wcsncmp(cmd_line, program_dir, ret))
    return true;

  return false;
}

bool InitNativeRegApi() {
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");

  // Setup the global function pointers for registry access.
  g_rtl_init_unicode_string = reinterpret_cast<RtlInitUnicodeStringFunction>(
      ::GetProcAddress(ntdll, "RtlInitUnicodeString"));

  g_nt_create_key = reinterpret_cast<NtCreateKeyFunction>(
      ::GetProcAddress(ntdll, "NtCreateKey"));

  g_nt_delete_key = reinterpret_cast<NtDeleteKeyFunction>(
      ::GetProcAddress(ntdll, "NtDeleteKey"));

  g_nt_open_key_ex = reinterpret_cast<NtOpenKeyExFunction>(
      ::GetProcAddress(ntdll, "NtOpenKeyEx"));

  g_nt_close =
      reinterpret_cast<NtCloseFunction>(::GetProcAddress(ntdll, "NtClose"));

  g_nt_query_value_key = reinterpret_cast<NtQueryValueKeyFunction>(
      ::GetProcAddress(ntdll, "NtQueryValueKey"));

  g_nt_set_value_key = reinterpret_cast<NtSetValueKeyFunction>(
      ::GetProcAddress(ntdll, "NtSetValueKey"));

  if (!g_rtl_init_unicode_string || !g_nt_create_key || !g_nt_open_key_ex ||
      !g_nt_delete_key || !g_nt_close || !g_nt_query_value_key ||
      !g_nt_set_value_key)
    return false;

  // We need to set HKCU based on the sid of the current user account.
  RtlFormatCurrentUserKeyPathFunction rtl_current_user_string =
      reinterpret_cast<RtlFormatCurrentUserKeyPathFunction>(
          ::GetProcAddress(ntdll, "RtlFormatCurrentUserKeyPath"));

  RtlFreeUnicodeStringFunction rtl_free_unicode_str =
      reinterpret_cast<RtlFreeUnicodeStringFunction>(
          ::GetProcAddress(ntdll, "RtlFreeUnicodeString"));

  if (!rtl_current_user_string || !rtl_free_unicode_str)
    return false;

  UNICODE_STRING current_user_reg_path;
  if (!NT_SUCCESS(rtl_current_user_string(&current_user_reg_path)))
    return false;

  // Finish setting up global HKCU path.
  ::wcsncat(g_kRegPathHKCU, current_user_reg_path.Buffer, (g_kMaxPathLen - 1));
  ::wcsncat(g_kRegPathHKCU, L"\\",
            (g_kMaxPathLen - ::wcslen(g_kRegPathHKCU) - 1));
  // Keep the sid string as well.
  wchar_t* ptr = ::wcsrchr(current_user_reg_path.Buffer, L'\\');
  ptr++;
  ::wcsncpy(g_current_user_sid_string, ptr, (g_kMaxPathLen - 1));
  rtl_free_unicode_str(&current_user_reg_path);

  // Figure out if we're a system or user install.
  g_system_install = IsThisProcSystem();

  g_initialized = true;
  return true;
}

const wchar_t* ConvertRootKey(nt::ROOT_KEY root) {
  nt::ROOT_KEY key = root;

  if (!root) {
    // AUTO
    key = g_system_install ? nt::HKLM : nt::HKCU;
  }

  if ((key == nt::HKCU) && (::wcslen(nt::HKCU_override) != 0)) {
    std::wstring temp(g_kRegPathHKCU);
    temp.append(nt::HKCU_override);
    temp.append(L"\\");
    ::wcsncpy(g_override_path, temp.c_str(), g_kMaxPathLen - 1);
    g_reg_redirection = true;
    return g_override_path;
  } else if ((key == nt::HKLM) && (::wcslen(nt::HKLM_override) != 0)) {
    std::wstring temp(g_kRegPathHKCU);
    temp.append(nt::HKLM_override);
    temp.append(L"\\");
    ::wcsncpy(g_override_path, temp.c_str(), g_kMaxPathLen - 1);
    g_reg_redirection = true;
    return g_override_path;
  }

  g_reg_redirection = false;
  if (key == nt::HKCU)
    return g_kRegPathHKCU;
  else
    return g_kRegPathHKLM;
}

// Turns a root and subkey path into the registry base hive and the rest of the
// subkey tokens.
// - |converted_root| should come directly out of ConvertRootKey function.
// - E.g. base hive: "\Registry\Machine\", "\Registry\User\<SID>\".
bool ParseFullRegPath(const wchar_t* converted_root,
                      const wchar_t* subkey_path,
                      std::wstring* out_base,
                      std::vector<std::wstring>* subkeys) {
  out_base->clear();
  subkeys->clear();
  std::wstring temp = L"";

  if (g_reg_redirection) {
    // Why process |converted_root|?  To handle reg redirection used by tests.
    // E.g.:
    // |converted_root| = "\REGISTRY\USER\S-1-5-21-39260824-743453154-142223018-
    // 716772\Software\Chromium\TempTestKeys\13110669370890870$94c6ed9d-bc34-
    // 44f3-a0b3-9eee2d3f2f82\".
    // |subkey_path| = "SOFTWARE\Google\Chrome\BrowserSec".
    temp.append(converted_root);
  }
  if (subkey_path != nullptr)
    temp.append(subkey_path);

  // Tokenize the full path.
  size_t find_start = 0;
  size_t delimiter = temp.find_first_of(L'\\');
  while (delimiter != std::wstring::npos) {
    std::wstring token = temp.substr(find_start, delimiter - find_start);
    if (!token.empty())
      subkeys->push_back(token);
    find_start = delimiter + 1;
    delimiter = temp.find_first_of(L'\\', find_start);
  }
  if (!temp.empty() && find_start < temp.length())
    // Get the last token.
    subkeys->push_back(temp.substr(find_start));

  if (g_reg_redirection) {
    // The base hive for HKCU needs to include the user SID.
    uint32_t num_base_tokens = 2;
    const wchar_t* hkcu = L"\\REGISTRY\\USER\\";
    if (0 == ::wcsnicmp(converted_root, hkcu, ::wcslen(hkcu)))
      num_base_tokens = 3;

    if (subkeys->size() < num_base_tokens)
      return false;

    // Pull out the base hive tokens.
    out_base->push_back(L'\\');
    for (size_t i = 0; i < num_base_tokens; i++) {
      out_base->append((*subkeys)[i].c_str());
      out_base->push_back(L'\\');
    }
    subkeys->erase(subkeys->begin(), subkeys->begin() + num_base_tokens);
  } else {
    out_base->assign(converted_root);
  }

  return true;
}

NTSTATUS CreateKeyWrapper(const std::wstring& key_path,
                          ACCESS_MASK access,
                          HANDLE* out_handle,
                          ULONG* create_or_open OPTIONAL) {
  UNICODE_STRING key_path_uni = {};
  g_rtl_init_unicode_string(&key_path_uni, key_path.c_str());

  OBJECT_ATTRIBUTES obj = {};
  InitializeObjectAttributes(&obj, &key_path_uni, OBJ_CASE_INSENSITIVE, NULL,
                             nullptr);

  return g_nt_create_key(out_handle, access, &obj, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, create_or_open);
}

}  // namespace

namespace nt {

const size_t g_kRegMaxPathLen = 255;
wchar_t HKLM_override[g_kRegMaxPathLen] = L"";
wchar_t HKCU_override[g_kRegMaxPathLen] = L"";

//------------------------------------------------------------------------------
// Create, open, delete, close functions
//------------------------------------------------------------------------------

bool CreateRegKey(ROOT_KEY root,
                  const wchar_t* key_path,
                  ACCESS_MASK access,
                  HANDLE* out_handle OPTIONAL) {
  if (!g_initialized)
    InitNativeRegApi();

  std::wstring current_path;
  std::vector<std::wstring> subkeys;
  if (!ParseFullRegPath(ConvertRootKey(root), key_path, &current_path,
                        &subkeys))
    return false;

  // Open the base hive first.  It should always exist already.
  HANDLE last_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status =
      CreateKeyWrapper(current_path, access, &last_handle, nullptr);
  if (!NT_SUCCESS(status))
    return false;

  size_t subkeys_size = subkeys.size();
  if (subkeys_size != 0)
    g_nt_close(last_handle);

  // Recursively open/create each subkey.
  std::vector<HANDLE> rollback;
  bool success = true;
  for (size_t i = 0; i < subkeys_size; i++) {
    current_path.append(subkeys[i]);
    current_path.push_back(L'\\');

    // Process the latest subkey.
    ULONG created = 0;
    HANDLE key_handle = INVALID_HANDLE_VALUE;
    status =
        CreateKeyWrapper(current_path.c_str(), access, &key_handle, &created);
    if (!NT_SUCCESS(status)) {
      success = false;
      break;
    }

    if (i == subkeys_size - 1) {
      last_handle = key_handle;
    } else {
      // Save any subkey handle created, in case of rollback.
      if (created == REG_CREATED_NEW_KEY)
        rollback.push_back(key_handle);
      else
        g_nt_close(key_handle);
    }
  }

  if (!success) {
    // Delete any subkeys created.
    for (HANDLE handle : rollback) {
      g_nt_delete_key(handle);
    }
  }
  for (HANDLE handle : rollback) {
    // Close the rollback handles, on success or failure.
    g_nt_close(handle);
  }
  if (!success)
    return false;

  // See if caller wants the handle left open.
  if (out_handle)
    *out_handle = last_handle;
  else
    g_nt_close(last_handle);

  return true;
}

bool OpenRegKey(ROOT_KEY root,
                const wchar_t* key_path,
                ACCESS_MASK access,
                HANDLE* out_handle,
                NTSTATUS* error_code OPTIONAL) {
  if (!g_initialized)
    InitNativeRegApi();

  NTSTATUS status = STATUS_UNSUCCESSFUL;
  UNICODE_STRING key_path_uni = {};
  OBJECT_ATTRIBUTES obj = {};
  *out_handle = INVALID_HANDLE_VALUE;

  std::wstring full_path(ConvertRootKey(root));
  full_path.append(key_path);

  g_rtl_init_unicode_string(&key_path_uni, full_path.c_str());
  InitializeObjectAttributes(&obj, &key_path_uni, OBJ_CASE_INSENSITIVE, NULL,
                             NULL);

  status = g_nt_open_key_ex(out_handle, access, &obj, 0);
  // See if caller wants the NTSTATUS.
  if (error_code)
    *error_code = status;

  if (NT_SUCCESS(status))
    return true;

  return false;
}

bool DeleteRegKey(HANDLE key) {
  if (!g_initialized)
    InitNativeRegApi();

  NTSTATUS status = STATUS_UNSUCCESSFUL;

  status = g_nt_delete_key(key);

  if (NT_SUCCESS(status))
    return true;

  return false;
}

// wrapper function
bool DeleteRegKey(ROOT_KEY root, const wchar_t* key_path) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, DELETE, &key, nullptr))
    return false;

  if (!DeleteRegKey(key)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

void CloseRegKey(HANDLE key) {
  if (!g_initialized)
    InitNativeRegApi();
  g_nt_close(key);
}

//------------------------------------------------------------------------------
// Getter functions
//------------------------------------------------------------------------------

bool QueryRegKeyValue(HANDLE key,
                      const wchar_t* value_name,
                      ULONG* out_type,
                      BYTE** out_buffer,
                      DWORD* out_size) {
  if (!g_initialized)
    InitNativeRegApi();

  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
  UNICODE_STRING value_uni = {};
  g_rtl_init_unicode_string(&value_uni, value_name);
  DWORD size_needed = 0;
  bool success = false;

  // First call to find out how much room we need for the value!
  ntstatus = g_nt_query_value_key(key, &value_uni, KeyValueFullInformation,
                                  nullptr, 0, &size_needed);
  if (ntstatus != STATUS_BUFFER_TOO_SMALL)
    return false;

  KEY_VALUE_FULL_INFORMATION* value_info =
      reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(new BYTE[size_needed]);

  // Second call to get the value.
  ntstatus = g_nt_query_value_key(key, &value_uni, KeyValueFullInformation,
                                  value_info, size_needed, &size_needed);
  if (NT_SUCCESS(ntstatus)) {
    *out_type = value_info->Type;
    *out_size = value_info->DataLength;
    *out_buffer = new BYTE[*out_size];
    ::memcpy(*out_buffer,
             (reinterpret_cast<BYTE*>(value_info) + value_info->DataOffset),
             *out_size);
    success = true;
  }

  delete[] value_info;
  return success;
}

// wrapper function
bool QueryRegValueDWORD(HANDLE key,
                        const wchar_t* value_name,
                        DWORD* out_dword) {
  ULONG type = REG_NONE;
  BYTE* value_bytes = nullptr;
  DWORD ret_size = 0;

  if (!QueryRegKeyValue(key, value_name, &type, &value_bytes, &ret_size) ||
      type != REG_DWORD)
    return false;

  *out_dword = *(reinterpret_cast<DWORD*>(value_bytes));

  delete[] value_bytes;
  return true;
}

// wrapper function
bool QueryRegValueDWORD(ROOT_KEY root,
                        const wchar_t* key_path,
                        const wchar_t* value_name,
                        DWORD* out_dword) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key,
                  NULL))
    return false;

  if (!QueryRegValueDWORD(key, value_name, out_dword)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

// wrapper function
bool QueryRegValueSZ(HANDLE key,
                     const wchar_t* value_name,
                     std::wstring* out_sz) {
  BYTE* value_bytes = nullptr;
  DWORD ret_size = 0;
  ULONG type = REG_NONE;

  if (!QueryRegKeyValue(key, value_name, &type, &value_bytes, &ret_size) ||
      type != REG_SZ)
    return false;

  *out_sz = reinterpret_cast<wchar_t*>(value_bytes);

  delete[] value_bytes;
  return true;
}

// wrapper function
bool QueryRegValueSZ(ROOT_KEY root,
                     const wchar_t* key_path,
                     const wchar_t* value_name,
                     std::wstring* out_sz) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key,
                  NULL))
    return false;

  if (!QueryRegValueSZ(key, value_name, out_sz)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

// wrapper function
bool QueryRegValueMULTISZ(HANDLE key,
                          const wchar_t* value_name,
                          std::vector<std::wstring>* out_multi_sz) {
  BYTE* value_bytes = nullptr;
  DWORD ret_size = 0;
  ULONG type = REG_NONE;

  if (!QueryRegKeyValue(key, value_name, &type, &value_bytes, &ret_size) ||
      type != REG_MULTI_SZ)
    return false;

  // Make sure the vector is empty to start.
  (*out_multi_sz).resize(0);

  wchar_t* pointer = reinterpret_cast<wchar_t*>(value_bytes);
  std::wstring temp = pointer;
  // Loop.  Each string is separated by '\0'.  Another '\0' at very end (so 2 in
  // a row).
  while (temp.length() != 0) {
    (*out_multi_sz).push_back(temp);

    pointer += temp.length() + 1;
    temp = pointer;
  }

  // Handle the case of "empty multi_sz".
  if (out_multi_sz->size() == 0)
    out_multi_sz->push_back(L"");

  delete[] value_bytes;
  return true;
}

// wrapper function
bool QueryRegValueMULTISZ(ROOT_KEY root,
                          const wchar_t* key_path,
                          const wchar_t* value_name,
                          std::vector<std::wstring>* out_multi_sz) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key,
                  NULL))
    return false;

  if (!QueryRegValueMULTISZ(key, value_name, out_multi_sz)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

//------------------------------------------------------------------------------
// Setter functions
//------------------------------------------------------------------------------

bool SetRegKeyValue(HANDLE key,
                    const wchar_t* value_name,
                    ULONG type,
                    const BYTE* data,
                    DWORD data_size) {
  if (!g_initialized)
    InitNativeRegApi();

  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
  UNICODE_STRING value_uni = {};
  g_rtl_init_unicode_string(&value_uni, value_name);

  BYTE* non_const_data = const_cast<BYTE*>(data);
  ntstatus =
      g_nt_set_value_key(key, &value_uni, 0, type, non_const_data, data_size);

  if (NT_SUCCESS(ntstatus))
    return true;

  return false;
}

// wrapper function
bool SetRegValueDWORD(HANDLE key, const wchar_t* value_name, DWORD value) {
  return SetRegKeyValue(key, value_name, REG_DWORD,
                        reinterpret_cast<BYTE*>(&value), sizeof(value));
}

// wrapper function
bool SetRegValueDWORD(ROOT_KEY root,
                      const wchar_t* key_path,
                      const wchar_t* value_name,
                      DWORD value) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_SET_VALUE | KEY_WOW64_32KEY, &key, NULL))
    return false;

  if (!SetRegValueDWORD(key, value_name, value)) {
    CloseRegKey(key);
    return false;
  }

  return true;
}

// wrapper function
bool SetRegValueSZ(HANDLE key,
                   const wchar_t* value_name,
                   const std::wstring& value) {
  // Make sure the number of bytes in |value|, including EoS, fits in a DWORD.
  if (std::numeric_limits<DWORD>::max() <
      ((value.length() + 1) * sizeof(wchar_t)))
    return false;

  DWORD size = (static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t)));
  return SetRegKeyValue(key, value_name, REG_SZ,
                        reinterpret_cast<const BYTE*>(value.c_str()), size);
}

// wrapper function
bool SetRegValueSZ(ROOT_KEY root,
                   const wchar_t* key_path,
                   const wchar_t* value_name,
                   const std::wstring& value) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_SET_VALUE | KEY_WOW64_32KEY, &key, NULL))
    return false;

  if (!SetRegValueSZ(key, value_name, value)) {
    CloseRegKey(key);
    return false;
  }

  return true;
}

// wrapper function
bool SetRegValueMULTISZ(HANDLE key,
                        const wchar_t* value_name,
                        const std::vector<std::wstring>& values) {
  std::vector<wchar_t> builder;

  for (auto& string : values) {
    // Just in case someone is passing in an illegal empty string
    // (not allowed in REG_MULTI_SZ), ignore it.
    if (!string.empty()) {
      for (const wchar_t& w : string) {
        builder.push_back(w);
      }
      builder.push_back(L'\0');
    }
  }
  // Add second null terminator to end REG_MULTI_SZ.
  builder.push_back(L'\0');
  // Handle rare case where the vector passed in was empty,
  // or only had an empty string.
  if (builder.size() == 1)
    builder.push_back(L'\0');

  if (std::numeric_limits<DWORD>::max() < builder.size())
    return false;

  return SetRegKeyValue(
      key, value_name, REG_MULTI_SZ, reinterpret_cast<BYTE*>(builder.data()),
      (static_cast<DWORD>(builder.size()) + 1) * sizeof(wchar_t));
}

// wrapper function
bool SetRegValueMULTISZ(ROOT_KEY root,
                        const wchar_t* key_path,
                        const wchar_t* value_name,
                        const std::vector<std::wstring>& values) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_SET_VALUE | KEY_WOW64_32KEY, &key, NULL))
    return false;

  if (!SetRegValueMULTISZ(key, value_name, values)) {
    CloseRegKey(key);
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
// Utils
//------------------------------------------------------------------------------

const wchar_t* GetCurrentUserSidString() {
  if (!g_initialized)
    InitNativeRegApi();

  return g_current_user_sid_string;
}

};  // namespace nt
