// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <sstream>

#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/policy_path_parser.h"
#include "chrome/install_static/user_data_dir.h"
#include "chrome_elf/nt_registry/nt_registry.h"

namespace install_static {

ProcessType g_process_type = ProcessType::UNINITIALIZED;

const wchar_t kRegValueChromeStatsSample[] = L"UsageStatsInSample";

// TODO(ananta)
// http://crbug.com/604923
// The constants defined in this file are also defined in chrome/installer and
// other places. we need to unify them.
const wchar_t kHeadless[] = L"CHROME_HEADLESS";
const wchar_t kShowRestart[] = L"CHROME_CRASHED";
const wchar_t kRestartInfo[] = L"CHROME_RESTART";
const wchar_t kRtlLocale[] = L"RIGHT_TO_LEFT";

const wchar_t kCrashpadHandler[] = L"crashpad-handler";
const wchar_t kProcessType[] = L"type";
const wchar_t kUserDataDirSwitch[] = L"user-data-dir";
const wchar_t kUtilityProcess[] = L"utility";

namespace {

// TODO(ananta)
// http://crbug.com/604923
// The constants defined in this file are also defined in chrome/installer and
// other places. we need to unify them.
// Chrome channel display names.
constexpr wchar_t kChromeChannelDev[] = L"dev";
constexpr wchar_t kChromeChannelBeta[] = L"beta";
constexpr wchar_t kChromeChannelStableExplicit[] = L"stable";

// TODO(ananta)
// http://crbug.com/604923
// These constants are defined in the chrome/installer directory as well. We
// need to unify them.
constexpr wchar_t kRegValueAp[] = L"ap";
constexpr wchar_t kRegValueUsageStats[] = L"usagestats";
constexpr wchar_t kMetricsReportingEnabled[] = L"MetricsReportingEnabled";

constexpr wchar_t kBrowserCrashDumpMetricsSubKey[] =
    L"\\BrowserCrashDumpAttempts";

void Trace(const wchar_t* format_string, ...) {
  static const int kMaxLogBufferSize = 1024;
  static wchar_t buffer[kMaxLogBufferSize] = {};

  va_list args = {};

  va_start(args, format_string);
  vswprintf(buffer, kMaxLogBufferSize, format_string, args);
  OutputDebugStringW(buffer);
  va_end(args);
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
  BOOL query_result = VerQueryValueW(
      version_resource, L"\\VarFileInfo\\Translation",
      reinterpret_cast<void**>(&translation_info), &data_size_in_bytes);
  if (!query_result)
    return false;

  *language = translation_info->language;
  *code_page = translation_info->code_page;
  return true;
}

bool GetValueFromVersionResource(const char* version_resource,
                                 const std::wstring& name,
                                 std::wstring* value_str) {
  assert(value_str);
  value_str->clear();

  // TODO(ananta)
  // It may be better in the long run to enumerate the languages and code pages
  // in the version resource and return the value from the first match.
  WORD language = 0;
  WORD code_page = 0;
  if (!GetLanguageAndCodePageFromVersionResource(version_resource, &language,
                                                 &code_page)) {
    return false;
  }

  const size_t array_size = 8;
  WORD lang_codepage[array_size] = {};
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

  static_assert((array_size % 2) == 0,
                "Language code page size should be a multiple of 2");
  assert(array_size == i);

  for (i = 0; i < array_size;) {
    wchar_t sub_block[MAX_PATH];
    WORD language = lang_codepage[i++];
    WORD code_page = lang_codepage[i++];
    _snwprintf_s(sub_block, MAX_PATH, MAX_PATH,
                 L"\\StringFileInfo\\%04hx%04hx\\%ls", language, code_page,
                 name.c_str());
    void* value = nullptr;
    uint32_t size = 0;
    BOOL r = ::VerQueryValueW(version_resource, sub_block, &value, &size);
    if (r && value) {
      value_str->assign(static_cast<wchar_t*>(value));
      return true;
    }
  }
  return false;
}

bool DirectoryExists(const std::wstring& path) {
  DWORD file_attributes = ::GetFileAttributes(path.c_str());
  if (file_attributes == INVALID_FILE_ATTRIBUTES)
    return false;
  return (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring GetChromeInstallRegistryPath() {
  std::wstring registry_path = L"Software\\";
  return AppendChromeInstallSubDirectory(
      InstallDetails::Get().mode(), true /* include_suffix */, &registry_path);
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
bool MatchPatternImpl(const std::wstring& source,
                      const std::wstring& pattern,
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
    return MatchPatternImpl(source, pattern, source_index + 1, pattern_index) ||
           MatchPatternImpl(source, pattern, source_index, pattern_index + 1);
  }
  return false;
}

// Defines the type of whitespace characters typically found in strings.
constexpr char kWhiteSpaces[] = " \t\n\r\f\v";
constexpr wchar_t kWhiteSpaces16[] = L" \t\n\r\f\v";

// Define specializations for white spaces based on the type of the string.
template <class StringType>
StringType GetWhiteSpacesForType();
template <>
std::wstring GetWhiteSpacesForType() {
  return kWhiteSpaces16;
}
template <>
std::string GetWhiteSpacesForType() {
  return kWhiteSpaces;
}

// Trim whitespaces from left & right
template <class StringType>
void TrimT(StringType* str) {
  str->erase(str->find_last_not_of(GetWhiteSpacesForType<StringType>()) + 1);
  str->erase(0, str->find_first_not_of(GetWhiteSpacesForType<StringType>()));
}

// Tokenizes a string based on a single character delimiter.
template <class StringType>
std::vector<StringType> TokenizeStringT(
    const StringType& str,
    typename StringType::value_type delimiter,
    bool trim_spaces) {
  std::vector<StringType> tokens;
  std::basic_istringstream<typename StringType::value_type> buffer(str);
  for (StringType token; std::getline(buffer, token, delimiter);) {
    if (trim_spaces)
      TrimT<StringType>(&token);
    tokens.push_back(token);
  }
  return tokens;
}

std::wstring ChannelFromAdditionalParameters(const InstallConstants& mode,
                                             bool system_level,
                                             bool binaries) {
  assert(kUseGoogleUpdateIntegration);
  // InitChannelInfo in google_update_settings.cc only reports a failure in the
  // case of multi-install Chrome where the binaries' ClientState key exists,
  // but that the "ap" value therein cannot be read due to some reason *other*
  // than it not being present. This should be exceedingly rare. For
  // simplicity's sake, use an empty |value| in case of any error whatsoever
  // here.
  std::wstring value;
  nt::QueryRegValueSZ(system_level ? nt::HKLM : nt::HKCU, nt::WOW6432,
                      (binaries ? GetBinariesClientStateKeyPath()
                                : GetClientStateKeyPath(mode.app_guid))
                          .c_str(),
                      kRegValueAp, &value);

  static constexpr wchar_t kChromeChannelBetaPattern[] = L"1?1-*";
  static constexpr wchar_t kChromeChannelBetaX64Pattern[] = L"*x64-beta*";
  static constexpr wchar_t kChromeChannelDevPattern[] = L"2?0-d*";
  static constexpr wchar_t kChromeChannelDevX64Pattern[] = L"*x64-dev*";

  std::transform(value.begin(), value.end(), value.begin(), ::tolower);

  // Empty channel names or those containing "stable" should be reported as
  // an empty string.
  std::wstring channel_name;
  if (value.empty() ||
      (value.find(kChromeChannelStableExplicit) != std::wstring::npos)) {
  } else if (MatchPattern(value, kChromeChannelDevPattern) ||
             MatchPattern(value, kChromeChannelDevX64Pattern)) {
    channel_name.assign(kChromeChannelDev);
  } else if (MatchPattern(value, kChromeChannelBetaPattern) ||
             MatchPattern(value, kChromeChannelBetaX64Pattern)) {
    channel_name.assign(kChromeChannelBeta);
  }
  // Else report values with garbage as stable since they will match the stable
  // rules in the update configs. ChannelInfo::GetChannelName painstakingly
  // strips off known modifiers (e.g., "-multi-full") to see if the empty string
  // remains, returning channel "unknown" if not. This differs here in that some
  // clients will tag crashes as "stable" rather than "unknown" via this
  // codepath, but it is an accurate reflection of which update channel the
  // client is on according to the server-side rules.

  return channel_name;
}

}  // namespace

bool IsSystemInstall() {
  return InstallDetails::Get().system_level();
}

bool IsMultiInstall() {
  return InstallDetails::Get().multi_install();
}

bool GetCollectStatsConsent() {
  bool enabled = true;

  if (ReportingIsEnforcedByPolicy(&enabled))
    return enabled;

  const bool system_install = IsSystemInstall();

  DWORD out_value = 0;

  // If system_install, first try ClientStateMedium in HKLM.
  if (system_install &&
      nt::QueryRegValueDWORD(
          nt::HKLM, nt::WOW6432,
          InstallDetails::Get().GetClientStateMediumKeyPath(true).c_str(),
          kRegValueUsageStats, &out_value)) {
    return (out_value == 1);
  }

  // Second, try ClientState.
  return (nt::QueryRegValueDWORD(
              system_install ? nt::HKLM : nt::HKCU, nt::WOW6432,
              InstallDetails::Get().GetClientStateKeyPath(true).c_str(),
              kRegValueUsageStats, &out_value) &&
          out_value == 1);
}

bool GetCollectStatsInSample() {
  std::wstring registry_path = GetChromeInstallRegistryPath();

  DWORD out_value = 0;
  if (!nt::QueryRegValueDWORD(nt::HKCU, nt::WOW6432, registry_path.c_str(),
                              kRegValueChromeStatsSample, &out_value)) {
    // If reading the value failed, treat it as though sampling isn't in effect,
    // implicitly meaning this install is in the sample.
    return true;
  }
  return out_value == 1;
}

bool SetCollectStatsInSample(bool in_sample) {
  std::wstring registry_path = GetChromeInstallRegistryPath();

  HANDLE key_handle = INVALID_HANDLE_VALUE;
  if (!nt::CreateRegKey(nt::HKCU, registry_path.c_str(),
                        KEY_SET_VALUE | KEY_WOW64_32KEY, &key_handle)) {
    return false;
  }

  bool success = nt::SetRegValueDWORD(key_handle, kRegValueChromeStatsSample,
                                      in_sample ? 1 : 0);
  nt::CloseRegKey(key_handle);
  return success;
}

// Appends "[kCompanyPathName\]kProductPathName[install_suffix]" to |path|,
// returning a reference to |path|.
std::wstring& AppendChromeInstallSubDirectory(const InstallConstants& mode,
                                              bool include_suffix,
                                              std::wstring* path) {
  if (*kCompanyPathName) {
    path->append(kCompanyPathName);
    path->push_back(L'\\');
  }
  path->append(kProductPathName, kProductPathNameLength);
  if (!include_suffix)
    return *path;
  return path->append(mode.install_suffix);
}

bool ReportingIsEnforcedByPolicy(bool* crash_reporting_enabled) {
  std::wstring policies_path = L"SOFTWARE\\Policies\\";
  AppendChromeInstallSubDirectory(InstallDetails::Get().mode(),
                                  false /* !include_suffix */, &policies_path);
  DWORD value = 0;

  // First, try HKLM.
  if (nt::QueryRegValueDWORD(nt::HKLM, nt::NONE, policies_path.c_str(),
                             kMetricsReportingEnabled, &value)) {
    *crash_reporting_enabled = (value != 0);
    return true;
  }

  // Second, try HKCU.
  if (nt::QueryRegValueDWORD(nt::HKCU, nt::NONE, policies_path.c_str(),
                             kMetricsReportingEnabled, &value)) {
    *crash_reporting_enabled = (value != 0);
    return true;
  }

  return false;
}

void InitializeProcessType() {
  assert(g_process_type == ProcessType::UNINITIALIZED);
  typedef bool (*IsSandboxedProcessFunc)();
  IsSandboxedProcessFunc is_sandboxed_process_func =
      reinterpret_cast<IsSandboxedProcessFunc>(
          ::GetProcAddress(::GetModuleHandle(nullptr), "IsSandboxedProcess"));
  if (is_sandboxed_process_func && is_sandboxed_process_func()) {
    g_process_type = ProcessType::NON_BROWSER_PROCESS;
    return;
  }

  // TODO(robertshield): Drop the command line check when we drop support for
  // enabling chrome_elf in unsandboxed processes.
  const wchar_t* command_line = GetCommandLine();
  if (command_line && ::wcsstr(command_line, L"--type")) {
    g_process_type = ProcessType::NON_BROWSER_PROCESS;
    return;
  }

  g_process_type = ProcessType::BROWSER_PROCESS;
}

bool IsNonBrowserProcess() {
  assert(g_process_type != ProcessType::UNINITIALIZED);
  return g_process_type == ProcessType::NON_BROWSER_PROCESS;
}

std::wstring GetCrashDumpLocation() {
  // In order to be able to start crash handling very early and in chrome_elf,
  // we cannot rely on chrome's PathService entries (for DIR_CRASH_DUMPS) being
  // available on Windows. See https://crbug.com/564398.
  std::wstring user_data_dir;
  bool ret = GetUserDataDirectory(&user_data_dir, nullptr);
  assert(ret);
  IgnoreUnused(ret);
  return user_data_dir.append(L"\\Crashpad");
}

std::string GetEnvironmentString(const std::string& variable_name) {
  return UTF16ToUTF8(
      GetEnvironmentString16(UTF8ToUTF16(variable_name).c_str()));
}

std::wstring GetEnvironmentString16(const wchar_t* variable_name) {
  DWORD value_length = ::GetEnvironmentVariableW(variable_name, nullptr, 0);
  if (!value_length)
    return std::wstring();
  std::wstring value(value_length, L'\0');
  value_length =
      ::GetEnvironmentVariableW(variable_name, &value[0], value_length);
  if (!value_length || value_length >= value.size())
    return std::wstring();
  value.resize(value_length);
  return value;
}

bool SetEnvironmentString(const std::string& variable_name,
                          const std::string& new_value) {
  return SetEnvironmentString16(UTF8ToUTF16(variable_name),
                                UTF8ToUTF16(new_value));
}

bool SetEnvironmentString16(const std::wstring& variable_name,
                            const std::wstring& new_value) {
  return !!SetEnvironmentVariable(variable_name.c_str(), new_value.c_str());
}

bool HasEnvironmentVariable(const std::string& variable_name) {
  return HasEnvironmentVariable16(UTF8ToUTF16(variable_name));
}

bool HasEnvironmentVariable16(const std::wstring& variable_name) {
  return !!::GetEnvironmentVariable(variable_name.c_str(), nullptr, 0);
}

void GetExecutableVersionDetails(const std::wstring& exe_path,
                                 std::wstring* product_name,
                                 std::wstring* version,
                                 std::wstring* special_build,
                                 std::wstring* channel_name) {
  assert(product_name);
  assert(version);
  assert(special_build);
  assert(channel_name);

  // Default values in case we don't find a version resource.
  *product_name = L"Chrome";
  *version = L"0.0.0.0-devel";
  special_build->clear();

  DWORD dummy = 0;
  DWORD length = ::GetFileVersionInfoSize(exe_path.c_str(), &dummy);
  if (length) {
    std::unique_ptr<char> data(new char[length]);
    if (::GetFileVersionInfo(exe_path.c_str(), dummy, length, data.get())) {
      GetValueFromVersionResource(data.get(), L"ProductVersion", version);

      std::wstring official_build;
      GetValueFromVersionResource(data.get(), L"Official Build",
                                  &official_build);
      if (official_build != L"1")
        version->append(L"-devel");
      GetValueFromVersionResource(data.get(), L"ProductShortName",
                                  product_name);
      GetValueFromVersionResource(data.get(), L"SpecialBuild", special_build);
    }
  }
  *channel_name = GetChromeChannelName(true /* add_modifier */);
}

std::wstring GetChromeChannelName(bool add_modifier) {
  const std::wstring& channel = InstallDetails::Get().channel();
  if (!add_modifier || !IsMultiInstall())
    return channel;
  if (channel.empty())
    return L"m";
  return channel + L"-m";
}

std::wstring GetBrowserCrashDumpAttemptsRegistryPath() {
  return GetChromeInstallRegistryPath().append(kBrowserCrashDumpMetricsSubKey);
}

bool MatchPattern(const std::wstring& source, const std::wstring& pattern) {
  assert(pattern.find(L"**") == std::wstring::npos);
  return MatchPatternImpl(source, pattern, 0, 0);
}

std::string UTF16ToUTF8(const std::wstring& source) {
  if (source.empty() ||
      static_cast<int>(source.size()) > std::numeric_limits<int>::max()) {
    return std::string();
  }
  int size = ::WideCharToMultiByte(CP_UTF8, 0, &source[0],
                                   static_cast<int>(source.size()), nullptr, 0,
                                   nullptr, nullptr);
  std::string result(size, '\0');
  if (::WideCharToMultiByte(CP_UTF8, 0, &source[0],
                            static_cast<int>(source.size()), &result[0], size,
                            nullptr, nullptr) != size) {
    assert(false);
    return std::string();
  }
  return result;
}

std::wstring UTF8ToUTF16(const std::string& source) {
  if (source.empty() ||
      static_cast<int>(source.size()) > std::numeric_limits<int>::max()) {
    return std::wstring();
  }
  int size = ::MultiByteToWideChar(CP_UTF8, 0, &source[0],
                                   static_cast<int>(source.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  if (::MultiByteToWideChar(CP_UTF8, 0, &source[0],
                            static_cast<int>(source.size()), &result[0],
                            size) != size) {
    assert(false);
    return std::wstring();
  }
  return result;
}

std::vector<std::string> TokenizeString(const std::string& str,
                                        char delimiter,
                                        bool trim_spaces) {
  return TokenizeStringT<std::string>(str, delimiter, trim_spaces);
}

std::vector<std::wstring> TokenizeString16(const std::wstring& str,
                                           wchar_t delimiter,
                                           bool trim_spaces) {
  return TokenizeStringT<std::wstring>(str, delimiter, trim_spaces);
}

std::wstring GetSwitchValueFromCommandLine(const std::wstring& command_line,
                                           const std::wstring& switch_name) {
  assert(!command_line.empty());
  assert(!switch_name.empty());

  std::wstring command_line_copy = command_line;
  // Remove leading and trailing spaces.
  TrimT<std::wstring>(&command_line_copy);

  // Find the switch in the command line. If we don't find the switch, return
  // an empty string.
  std::wstring switch_token = L"--";
  switch_token += switch_name;
  switch_token += L"=";
  size_t switch_offset = command_line_copy.find(switch_token);
  if (switch_offset == std::string::npos)
    return std::wstring();

  // The format is "--<switch name>=blah". Look for a space after the
  // "--<switch name>=" string. If we don't find a space assume that the switch
  // value ends at the end of the command line.
  size_t switch_value_start_offset = switch_offset + switch_token.length();
  if (std::wstring(kWhiteSpaces16).find(
          command_line_copy[switch_value_start_offset]) != std::wstring::npos) {
    switch_value_start_offset = command_line_copy.find_first_not_of(
        GetWhiteSpacesForType<std::wstring>(), switch_value_start_offset);
    if (switch_value_start_offset == std::wstring::npos)
      return std::wstring();
  }
  size_t switch_value_end_offset =
      command_line_copy.find_first_of(GetWhiteSpacesForType<std::wstring>(),
                                      switch_value_start_offset);
  if (switch_value_end_offset == std::wstring::npos)
    switch_value_end_offset = command_line_copy.length();

  std::wstring switch_value = command_line_copy.substr(
      switch_value_start_offset,
      switch_value_end_offset - (switch_offset + switch_token.length()));
  TrimT<std::wstring>(&switch_value);
  return switch_value;
}

bool RecursiveDirectoryCreate(const std::wstring& full_path) {
  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* full_path_str = full_path.c_str();
  DWORD file_attributes = ::GetFileAttributes(full_path_str);
  if (file_attributes != INVALID_FILE_ATTRIBUTES) {
    if ((file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      Trace(L"%hs( %ls directory exists )\n", __func__, full_path_str);
      return true;
    }
    Trace(L"%hs( %ls directory conflicts with an existing file. )\n", __func__,
          full_path_str);
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  std::wstring parent_path;
  std::size_t pos = full_path.find_last_of(L"/\\");
  if (pos != std::wstring::npos) {
    parent_path = full_path.substr(0, pos);
    if (!RecursiveDirectoryCreate(parent_path)) {
      Trace(L"Failed to create one of the parent directories");
      return false;
    }
  }
  if (!::CreateDirectory(full_path_str, nullptr)) {
    DWORD error_code = ::GetLastError();
    if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path_str)) {
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
  return true;
}

// This function takes these inputs rather than accessing the module's
// InstallDetails instance since it is used to bootstrap InstallDetails.
std::wstring DetermineChannel(const InstallConstants& mode,
                              bool system_level,
                              bool multi_install) {
  if (!kUseGoogleUpdateIntegration)
    return std::wstring();

  switch (mode.channel_strategy) {
    case ChannelStrategy::UNSUPPORTED:
      assert(false);
      break;
    case ChannelStrategy::ADDITIONAL_PARAMETERS:
      return ChannelFromAdditionalParameters(mode, system_level, multi_install);
    case ChannelStrategy::FIXED:
      return mode.default_channel_name;
  }

  return std::wstring();
}

}  // namespace install_static
