// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include <windows.h>
#include <assert.h>
#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "build/build_config.h"

namespace install_static {

ProcessType g_process_type = ProcessType::UNINITIALIZED;

// TODO(ananta)
// http://crbug.com/604923
// The constants defined in this file are also defined in chrome/installer and
// other places. we need to unify them.

// Chrome channel display names.
const wchar_t kChromeChannelUnknown[] = L"unknown";
const wchar_t kChromeChannelCanary[] = L"canary";
const wchar_t kChromeChannelDev[] = L"dev";
const wchar_t kChromeChannelBeta[] = L"beta";
const wchar_t kChromeChannelStable[] = L"";
const wchar_t kChromeChannelStableExplicit[] = L"stable";
const wchar_t kRegApField[] = L"ap";

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kRegPathClientState[] = L"Software\\Google\\Update\\ClientState";
const wchar_t kRegPathClientStateMedium[] =
L"Software\\Google\\Update\\ClientStateMedium";
const wchar_t kRegPathChromePolicy[] = L"SOFTWARE\\Policies\\Google\\Chrome";
#else
const wchar_t kRegPathClientState[] =
    L"Software\\Chromium\\Update\\ClientState";
const wchar_t kRegPathClientStateMedium[] =
    L"Software\\Chromium\\Update\\ClientStateMedium";
const wchar_t kRegPathChromePolicy[] = L"SOFTWARE\\Policies\\Chromium";
#endif

const wchar_t kRegValueUsageStats[] = L"usagestats";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kMetricsReportingEnabled[] = L"MetricsReportingEnabled";

const wchar_t kAppGuidCanary[] =
    L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
const wchar_t kAppGuidGoogleChrome[] =
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kAppGuidGoogleBinaries[] =
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

namespace {

// TODO(ananta)
// http://crbug.com/604923
// These constants are defined in the chrome/installer directory as well. We
// need to unify them.
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kSxSSuffix[] = L" SxS";
const wchar_t kGoogleChromeInstallSubDir1[] = L"Google";
const wchar_t kGoogleChromeInstallSubDir2[] = L"Chrome";
#else
const wchar_t kChromiumInstallSubDir[] = L"Chromium";
#endif  // defined(GOOGLE_CHROME_BUILD)

const wchar_t kUserDataDirname[] = L"User Data";
const wchar_t kBrowserCrashDumpMetricsSubKey[] = L"\\BrowserCrashDumpAttempts";

const wchar_t kRegPathGoogleUpdate[] = L"Software\\Google\\Update";
const wchar_t kRegGoogleUpdateVersion[] = L"version";

void Trace(const wchar_t* format_string, ...) {
  static const int kMaxLogBufferSize = 1024;
  static wchar_t buffer[kMaxLogBufferSize] = {};

  va_list args = {};

  va_start(args, format_string);
  vswprintf(buffer, arraysize(buffer), format_string, args);
  OutputDebugString(buffer);
  va_end(args);
}

// Reads a string value identified by |value_to_read| from the registry path
// under |key_path|. We look in HKLM or HKCU depending on whether the
// |system_install| parameter is true.
// Please note that this function only looks in the 32bit view of the registry.
bool ReadKeyValueString(bool system_install, const wchar_t* key_path,
                        const wchar_t* guid, const wchar_t* value_to_read,
                        base::string16* value_out) {
  HKEY key = NULL;
  value_out->clear();

  base::string16 full_key_path(key_path);
  if (guid && *guid) {
    full_key_path.append(1, L'\\');
    full_key_path.append(guid);
  }

  if (::RegOpenKeyEx(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                     full_key_path.c_str(), 0,
                     KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key) !=
                     ERROR_SUCCESS) {
    return false;
  }

  const size_t kMaxStringLength = 1024;
  wchar_t raw_value[kMaxStringLength] = {};
  DWORD size = sizeof(raw_value);
  DWORD type = REG_SZ;
  LONG result = ::RegQueryValueEx(key, value_to_read, 0, &type,
                                  reinterpret_cast<LPBYTE>(raw_value), &size);

  if (result == ERROR_SUCCESS) {
    if (type != REG_SZ || (size & 1) != 0) {
      result = ERROR_NOT_SUPPORTED;
    } else if (size == 0) {
      *raw_value = L'\0';
    } else if (raw_value[size / sizeof(wchar_t) - 1] != L'\0') {
      if ((size / sizeof(wchar_t)) < kMaxStringLength)
        raw_value[size / sizeof(wchar_t)] = L'\0';
      else
        result = ERROR_MORE_DATA;
    }
  }

  if (result == ERROR_SUCCESS)
    *value_out = raw_value;

  ::RegCloseKey(key);

  return result == ERROR_SUCCESS;
}

// Reads a DWORD value identified by |value_to_read| from the registry path
// under |key_path|. We look in HKLM or HKCU depending on whether the
// |system_install| parameter is true.
// Please note that this function only looks in the 32bit view of the registry.
bool ReadKeyValueDW(bool system_install, const wchar_t* key_path,
                    base::string16 guid, const wchar_t* value_to_read,
                    DWORD* value_out) {
  HKEY key = NULL;
  *value_out = 0;

  base::string16 full_key_path(key_path);
  full_key_path.append(1, L'\\');
  full_key_path.append(guid);

  if (::RegOpenKeyEx(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                     full_key_path.c_str(), 0,
                     KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key) !=
                     ERROR_SUCCESS) {
    return false;
  }

  DWORD size = sizeof(*value_out);
  DWORD type = REG_DWORD;
  LONG result = ::RegQueryValueEx(key, value_to_read, 0, &type,
                                  reinterpret_cast<BYTE*>(value_out), &size);

  ::RegCloseKey(key);

  return result == ERROR_SUCCESS && size == sizeof(*value_out);
}

bool GetLanguageAndCodePageFromVersionResource(const char* version_resource,
                                               WORD* language,
                                               WORD* code_page) {
  if (!version_resource)
    return false;

  struct LanguageAndCodePage {
    WORD language;
    WORD code_page;
  };

  LanguageAndCodePage* translation_info = nullptr;
  uint32_t data_size_in_bytes = 0;
  BOOL query_result = VerQueryValue(version_resource,
                                    L"\\VarFileInfo\\Translation",
                                    reinterpret_cast<void**>(&translation_info),
                                    &data_size_in_bytes);
  if (!query_result)
    return false;

  *language = translation_info->language;
  *code_page = translation_info->code_page;
  return true;
}

bool GetValueFromVersionResource(const char* version_resource,
                                 const base::string16& name,
                                 base::string16* value_str) {
  assert(value_str);
  value_str->clear();

  // TODO(ananta)
  // It may be better in the long run to enumerate the languages and code pages
  // in the version resource and return the value from the first match.
  WORD language = 0;
  WORD code_page = 0;
  if (!GetLanguageAndCodePageFromVersionResource(version_resource,
                                                 &language,
                                                 &code_page)) {
    return false;
  }

  WORD lang_codepage[8] = {};
  size_t i = 0;
  // Use the language and codepage
  lang_codepage[i++] = language;
  lang_codepage[i++] = code_page;
  // Use the default language and codepage from the resource.
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = code_page;
  // Use the language from the resource and Latin codepage (most common).
  lang_codepage[i++] = language;
  lang_codepage[i++] = 1252;
  // Use the default language and Latin codepage (most common).
  lang_codepage[i++] = ::GetUserDefaultLangID();
  lang_codepage[i++] = 1252;

  static_assert((arraysize(lang_codepage) % 2) == 0,
                "Language code page size should be a multiple of 2");
  assert(arraysize(lang_codepage) == i);

  for (i = 0; i < arraysize(lang_codepage);) {
    wchar_t sub_block[MAX_PATH];
    WORD language = lang_codepage[i++];
    WORD code_page = lang_codepage[i++];
    _snwprintf_s(sub_block, MAX_PATH, MAX_PATH,
        L"\\StringFileInfo\\%04hx%04hx\\%ls", language, code_page,
        name.c_str());
    void* value = nullptr;
    uint32_t size = 0;
    BOOL r = ::VerQueryValue(version_resource, sub_block, &value, &size);
    if (r && value) {
      value_str->assign(static_cast<wchar_t*>(value));
      return true;
    }
  }
  return false;
}

// Returns the executable path for the current process.
base::string16 GetCurrentProcessExePath() {
  wchar_t exe_path[MAX_PATH];
  if (GetModuleFileName(nullptr, exe_path, arraysize(exe_path)) == 0)
    return base::string16();
  return exe_path;
}

// UTF8 to UTF16 and vice versa conversion helpers. We cannot use the base
// string conversion utilities here as they bring about a dependency on
// user32.dll which is not allowed in this file.

// Convert a UTF16 string to an UTF8 string.
std::string utf16_to_utf8(const base::string16 &source) {
  if (source.empty())
    return std::string();
  int size = ::WideCharToMultiByte(CP_UTF8, 0, &source[0],
      static_cast<int>(source.size()), nullptr, 0, nullptr, nullptr);
  std::string result(size, '\0');
  if (::WideCharToMultiByte(CP_UTF8, 0, &source[0],
          static_cast<int>(source.size()), &result[0], size, nullptr,
          nullptr) != size) {
    assert(false);
    return std::string();
  }
  return result;
}

// Convert a UTF8 string to a UTF16 string.
base::string16 utf8_to_string16(const std::string &source) {
  if (source.empty())
    return base::string16();
  int size = ::MultiByteToWideChar(CP_UTF8, 0, &source[0],
      static_cast<int>(source.size()), nullptr, 0);
  base::string16 result(size, L'\0');
  if (::MultiByteToWideChar(CP_UTF8, 0, &source[0],
          static_cast<int>(source.size()), &result[0], size) != size) {
    assert(false);
    return base::string16();
  }
  return result;
}

bool RecursiveDirectoryCreate(const base::string16& full_path) {
  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* full_path_str = full_path.c_str();
  DWORD file_attributes = ::GetFileAttributes(full_path_str);
  if (file_attributes != INVALID_FILE_ATTRIBUTES) {
    if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      Trace(L"%hs( %ls directory exists )\n", __FUNCTION__, full_path_str);
      return true;
    }
    Trace(L"%hs( %ls directory conflicts with an existing file. )\n",
          __FUNCTION__, full_path_str);
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  base::string16 parent_path;
  std::size_t pos = full_path.find_last_of(L"/\\");
  if (pos != base::string16::npos) {
    parent_path = full_path.substr(0, pos);
    if (!RecursiveDirectoryCreate(parent_path)) {
      Trace(L"Failed to create one of the parent directories");
      return false;
    }
  }
  if (!::CreateDirectory(full_path_str, NULL)) {
    DWORD error_code = ::GetLastError();
    if (error_code == ERROR_ALREADY_EXISTS) {
      DWORD file_attributes = ::GetFileAttributes(full_path_str);
      if ((file_attributes != INVALID_FILE_ATTRIBUTES) &&
          ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
        // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we
        // were racing with someone creating the same directory, or a file
        // with the same path.  If the directory exists, we lost the
        // race to create the same directory.
        return true;
      } else {
        Trace(L"Failed to create directory %ls, last error is %d\n",
              full_path_str, error_code);
        return false;
      }
    }
  }
  return true;
}

bool GetCollectStatsConsentImpl(const base::string16& exe_path) {
  bool enabled = true;
  if (ReportingIsEnforcedByPolicy(&enabled))
    return enabled;

  bool system_install = IsSystemInstall(exe_path.c_str());
  base::string16 app_guid;

  if (IsSxSChrome(exe_path.c_str())) {
    app_guid = kAppGuidCanary;
  } else {
    app_guid = IsMultiInstall(system_install) ? kAppGuidGoogleBinaries :
        kAppGuidGoogleChrome;
  }

  DWORD out_value = 0;
  if (system_install &&
    ReadKeyValueDW(system_install, kRegPathClientStateMedium, app_guid,
                   kRegValueUsageStats, &out_value)) {
    return out_value == 1;
  }

  return ReadKeyValueDW(system_install, kRegPathClientState, app_guid,
      kRegValueUsageStats, &out_value) && out_value == 1;
}

// Returns true if the |source| string matches the |pattern|. The pattern
// may contain wildcards like '?' which matches one character or a '*'
// which matches 0 or more characters.
// Please note that pattern matches the whole string. If you want to find
// something in the middle of the string then you need to specify the pattern
// as '*xyz*'.
// |source_index| is the index of the current character being matched in
// |source|.
// |pattern_index| is the index of the current pattern character in |pattern|
// which is matched with source.
bool MatchPatternImpl(const base::string16& source,
                      const base::string16& pattern,
                      size_t source_index,
                      size_t pattern_index) {
  if (source.empty() && pattern.empty())
    return true;

  if (source_index > source.length() || pattern_index > pattern.length())
    return false;

  // If we reached the end of both strings, then we are done.
  if ((source_index == source.length()) &&
      (pattern_index == pattern.length())) {
    return true;
  }

  // If the current character in the pattern is a '*' then make sure that
  // characters after the pattern are present in the source string. This
  // assumes that you won't have two consecutive '*' characters in the pattern.
  if ((pattern[pattern_index] == L'*') &&
      (pattern_index + 1 < pattern.length()) &&
      (source_index >= source.length())) {
    return false;
  }

  // If the pattern contains wildcard characters '?' or '.' or there is a match
  // then move ahead in both strings.
  if ((pattern[pattern_index] == L'?') ||
      (pattern[pattern_index] == source[source_index])) {
    return MatchPatternImpl(source, pattern, source_index + 1,
                            pattern_index + 1);
  }

  // If we have a '*' then there are two possibilities
  // 1. We consider current character of source.
  // 2. We ignore current character of source.
  if (pattern[pattern_index] == L'*') {
    return MatchPatternImpl(source, pattern, source_index + 1,
                            pattern_index) ||
           MatchPatternImpl(source, pattern, source_index, pattern_index + 1);
  }
  return false;
}

}  // namespace

bool IsSxSChrome(const wchar_t* exe_path) {
  return wcsstr(exe_path, L"Chrome SxS\\Application") != NULL;
}

bool IsSystemInstall(const wchar_t* exe_path) {
  wchar_t program_dir[MAX_PATH] = {};
  DWORD ret = ::GetEnvironmentVariable(L"PROGRAMFILES", program_dir,
                                       arraysize(program_dir));
  if (ret && ret < arraysize(program_dir) &&
      !wcsnicmp(exe_path, program_dir, ret)) {
    return true;
  }

  ret = ::GetEnvironmentVariable(L"PROGRAMFILES(X86)", program_dir,
                                 arraysize(program_dir));
  if (ret && ret < arraysize(program_dir) &&
      !wcsnicmp(exe_path, program_dir, ret)) {
    return true;
  }

  return false;
}

bool IsMultiInstall(bool is_system_install) {
  base::string16 args;
  if (!ReadKeyValueString(is_system_install, kRegPathClientState,
                          kAppGuidGoogleChrome, kUninstallArgumentsField,
                          &args)) {
    return false;
  }
  return args.find(L"--multi-install") != base::string16::npos;
}

bool GetCollectStatsConsent() {
  return GetCollectStatsConsentImpl(GetCurrentProcessExePath());
}

bool GetCollectStatsConsentForTesting(const base::string16& exe_path) {
  return GetCollectStatsConsentImpl(exe_path);
}

bool ReportingIsEnforcedByPolicy(bool* metrics_is_enforced_by_policy) {
  HKEY key = NULL;
  DWORD value = 0;
  BYTE* value_bytes = reinterpret_cast<BYTE*>(&value);
  DWORD size = sizeof(value);
  DWORD type = REG_DWORD;

  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, kRegPathChromePolicy, 0,
                     KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
    if (::RegQueryValueEx(key, kMetricsReportingEnabled, 0, &type,
                          value_bytes, &size) == ERROR_SUCCESS) {
      *metrics_is_enforced_by_policy = value != 0;
    }
    ::RegCloseKey(key);
    return size == sizeof(value);
  }

  if (::RegOpenKeyEx(HKEY_CURRENT_USER, kRegPathChromePolicy, 0,
                     KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
    if (::RegQueryValueEx(key, kMetricsReportingEnabled, 0, &type,
                          value_bytes, &size) == ERROR_SUCCESS) {
      *metrics_is_enforced_by_policy = value != 0;
    }
    ::RegCloseKey(key);
    return size == sizeof(value);
  }

  return false;
}

void InitializeProcessType() {
  assert(g_process_type == ProcessType::UNINITIALIZED);
  typedef bool (*IsSandboxedProcessFunc)();
  IsSandboxedProcessFunc is_sandboxed_process_func =
      reinterpret_cast<IsSandboxedProcessFunc>(
          GetProcAddress(GetModuleHandle(NULL), "IsSandboxedProcess"));
  if (is_sandboxed_process_func && is_sandboxed_process_func()) {
    g_process_type = ProcessType::NON_BROWSER_PROCESS;
    return;
  }

  // TODO(robertshield): Drop the command line check when we drop support for
  // enabling chrome_elf in unsandboxed processes.
  const wchar_t* command_line = GetCommandLine();
  if (command_line && wcsstr(command_line, L"--type")) {
    g_process_type = ProcessType::NON_BROWSER_PROCESS;
    return;
  }

  g_process_type = ProcessType::BROWSER_PROCESS;
}

bool IsNonBrowserProcess() {
  assert(g_process_type != ProcessType::UNINITIALIZED);
  return g_process_type == ProcessType::NON_BROWSER_PROCESS;
}

bool GetDefaultUserDataDirectory(base::string16* result) {
  static const wchar_t kLocalAppData[] = L"%LOCALAPPDATA%";

  std::unique_ptr<wchar_t> user_data_dir_path;

  // This environment variable should be set on Windows 7 and up.
  // If we fail to find this variable then we default to the temporary files
  // path.
  DWORD size = ::ExpandEnvironmentStrings(kLocalAppData, nullptr, 0);
  if (size) {
    user_data_dir_path.reset(new wchar_t[size]);
    if (::ExpandEnvironmentStrings(kLocalAppData,
                                   user_data_dir_path.get(),
                                   size) != size) {
      user_data_dir_path.reset();
    }
  }
  // We failed to find the %LOCALAPPDATA% folder. Fallback to the temporary
  // files path. If we fail to find this we bail.
  if (!user_data_dir_path.get()) {
    size = ::GetTempPath(0, nullptr);
    if (!size)
      return false;
    user_data_dir_path.reset(new wchar_t[size + 1]);
    if (::GetTempPath(size + 1, user_data_dir_path.get()) != size)
      return false;
  }

  base::string16 install_sub_directory = GetChromeInstallSubDirectory();

  *result = user_data_dir_path.get();
  if ((*result)[result->length() - 1] != L'\\')
    result->append(L"\\");
  result->append(install_sub_directory);
  result->append(L"\\");
  result->append(kUserDataDirname);
  return true;
}

bool GetDefaultCrashDumpLocation(base::string16* crash_dir) {
  // In order to be able to start crash handling very early, we do not rely on
  // chrome's PathService entries (for DIR_CRASH_DUMPS) being available on
  // Windows. See https://crbug.com/564398.
  if (!GetDefaultUserDataDirectory(crash_dir))
    return false;

  // We have to make sure the user data dir exists on first run. See
  // http://crbug.com/591504.
  if (!RecursiveDirectoryCreate(crash_dir->c_str()))
    return false;
  crash_dir->append(L"\\Crashpad");
  return true;
}


std::string GetEnvironmentString(const std::string& variable_name) {
  DWORD value_length = ::GetEnvironmentVariable(
      utf8_to_string16(variable_name).c_str(), NULL, 0);
  if (value_length == 0)
    return std::string();
  std::unique_ptr<wchar_t[]> value(new wchar_t[value_length]);
  ::GetEnvironmentVariable(utf8_to_string16(variable_name).c_str(),
      value.get(), value_length);
  return utf16_to_utf8(value.get());
}

bool SetEnvironmentString(const std::string& variable_name,
                          const std::string& new_value) {
  return !!SetEnvironmentVariable(utf8_to_string16(variable_name).c_str(),
      utf8_to_string16(new_value).c_str());
}

bool HasEnvironmentVariable(const std::string& variable_name) {
  return !!::GetEnvironmentVariable(utf8_to_string16(variable_name).c_str(),
      NULL, 0);
}

bool GetExecutableVersionDetails(const base::string16& exe_path,
                                 base::string16* product_name,
                                 base::string16* version,
                                 base::string16* special_build,
                                 base::string16* channel_name) {
  assert(product_name);
  assert(version);
  assert(special_build);
  assert(channel_name);

  // Default values in case we don't find a version resource.
  *product_name = L"Chrome";
  *version = L"0.0.0.0-devel";
  *channel_name = kChromeChannelUnknown;
  special_build->clear();

  DWORD dummy = 0;
  DWORD length = ::GetFileVersionInfoSize(exe_path.c_str(), &dummy);
  if (length) {
    std::unique_ptr<char> data(new char[length]);
    if (::GetFileVersionInfo(exe_path.c_str(), dummy, length,
                             data.get())) {
      GetValueFromVersionResource(data.get(), L"ProductVersion", version);

      base::string16 official_build;
      GetValueFromVersionResource(data.get(), L"Official Build",
                                  &official_build);
      if (official_build != L"1")
        version->append(L"-devel");
      GetValueFromVersionResource(data.get(), L"ProductShortName",
                                  product_name);
      GetValueFromVersionResource(data.get(), L"SpecialBuild", special_build);
    }
  }
  GetChromeChannelName(!IsSystemInstall(exe_path.c_str()), true, channel_name);
  return true;
}

void GetChromeChannelName(bool is_per_user_install,
                          bool add_modifier,
                          base::string16* channel_name) {
  channel_name->clear();
  // TODO(ananta)
  // http://crbug.com/604923
  // Unify this with the chrome/installer/util/channel_info.h/.cc.
  if (IsSxSChrome(GetCurrentProcessExePath().c_str())) {
    *channel_name = L"canary";
  } else {
    base::string16 value;
    bool channel_available = false;
    bool is_multi_install = IsMultiInstall(!is_per_user_install);
    if (is_multi_install) {
      channel_available = ReadKeyValueString(!is_per_user_install,
                                             kRegPathClientState,
                                             kAppGuidGoogleBinaries,
                                             kRegApField,
                                             &value);
    } else {
      channel_available = ReadKeyValueString(!is_per_user_install,
                                             kRegPathClientState,
                                             kAppGuidGoogleChrome,
                                             kRegApField,
                                             &value);
    }
    if (channel_available) {
      static const wchar_t kChromeChannelBetaPattern[] = L"1?1-*";
      static const wchar_t kChromeChannelBetaX64Pattern[] = L"*x64-beta*";
      static const wchar_t kChromeChannelDevPattern[] = L"2?0-d*";
      static const wchar_t kChromeChannelDevX64Pattern[] = L"*x64-dev*";

      std::transform(value.begin(), value.end(), value.begin(), ::tolower);

      // Empty channel names or those containing "stable" should be reported as
      // an empty string. Exceptions being if |add_modifier| is true and this
      // is a multi install. In that case we report the channel name as "-m".
      if (value.empty() || (value.find(kChromeChannelStableExplicit)
              != base::string16::npos)) {
        if (add_modifier && is_multi_install)
          channel_name->append(L"-m");
        return;
      }

      if (MatchPattern(value, kChromeChannelDevPattern) ||
          MatchPattern(value, kChromeChannelDevX64Pattern)) {
        channel_name->assign(kChromeChannelDev);
      }

      if (MatchPattern(value, kChromeChannelBetaPattern) ||
          MatchPattern(value, kChromeChannelBetaX64Pattern)) {
        channel_name->assign(kChromeChannelBeta);
      }

      if (add_modifier && is_multi_install)
        channel_name->append(L"-m");

      // If we fail to find any matching pattern in the channel name then we
      // default to empty which means stable. Not sure if this is ok.
      // TODO(ananta)
      // http://crbug.com/604923
      // Check if this is ok.
    } else {
      *channel_name = kChromeChannelUnknown;
    }
  }
}

std::string GetGoogleUpdateVersion() {
  // TODO(ananta)
  // Consider whether Chromium should connect to Google update to manage
  // updates. Should this be returning an empty string for Chromium builds?.
  base::string16 update_version;
  if (ReadKeyValueString(IsSystemInstall(GetCurrentProcessExePath().c_str()),
                         kRegPathGoogleUpdate,
                         nullptr,
                         kRegGoogleUpdateVersion,
                         &update_version)) {
    return utf16_to_utf8(update_version);
  }
  return std::string();
}

base::string16 GetChromeInstallSubDirectory() {
#if defined(GOOGLE_CHROME_BUILD)
  base::string16 result = kGoogleChromeInstallSubDir1;
  result += L"\\";
  result += kGoogleChromeInstallSubDir2;
  if (IsSxSChrome(GetCurrentProcessExePath().c_str()))
    result += kSxSSuffix;
  return result;
#else
  return base::string16(kChromiumInstallSubDir);
#endif
}

base::string16 GetBrowserCrashDumpAttemptsRegistryPath() {
  base::string16 registry_path = L"Software\\";
  registry_path += GetChromeInstallSubDirectory();
  registry_path += kBrowserCrashDumpMetricsSubKey;
  return registry_path;
}

bool MatchPattern(const base::string16& source,
                  const base::string16& pattern) {
  assert(pattern.find(L"**") == base::string16::npos);
  return MatchPatternImpl(source, pattern, 0, 0);
}

}  // namespace install_static
