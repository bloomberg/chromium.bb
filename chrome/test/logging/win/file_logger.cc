// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/logging/win/file_logger.h"

#include <windows.h>
#include <guiddef.h>
#include <objbase.h>

#include <ios>

#include "base/debug/trace_event_win.h"
#include "base/logging.h"
#include "base/file_path.h"
#include "base/logging_win.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/registry.h"
#include "chrome/common/env_vars.h"

namespace logging_win {

namespace {

const wchar_t kChromeTestSession[] = L"chrome_tests";

// From chrome_tab.cc: {0562BFC3-2550-45b4-BD8E-A310583D3A6F}
const GUID kChromeFrameProvider =
    { 0x562bfc3, 0x2550, 0x45b4,
        { 0xbd, 0x8e, 0xa3, 0x10, 0x58, 0x3d, 0x3a, 0x6f } };

// From chrome/common/logging_chrome.cc: {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeTraceProviderName =
    { 0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

// {81729947-CD2A-49e6-8885-785429F339F5}
const GUID kChromeTestsProvider =
    { 0x81729947, 0xcd2a, 0x49e6,
        { 0x88, 0x85, 0x78, 0x54, 0x29, 0xf3, 0x39, 0xf5 } };

// The configurations for the supported providers.  This must be in sync with
// FileLogger::EventProviderBits.
const struct {
  const GUID* provider_name;
  uint8 level;
  uint32 flags;
} kProviders[] = {
  { &kChromeTraceProviderName, 255, 0 },
  { &kChromeFrameProvider, 255, 0 },
  { &kChromeTestsProvider, 255, 0 },
  { &base::debug::kChromeTraceProviderName, 255, 0 }
};

COMPILE_ASSERT((1 << arraysize(kProviders)) - 1 ==
                   FileLogger::kAllEventProviders,
               size_of_kProviders_is_inconsistent_with_kAllEventProviders);

// The provider bits that require CHROME_ETW_LOGGING in the environment.
const uint32 kChromeLogProviders =
    FileLogger::CHROME_LOG_PROVIDER | FileLogger::CHROME_FRAME_LOG_PROVIDER;
const HKEY kEnvironmentRoot = HKEY_LOCAL_MACHINE;
const wchar_t kEnvironmentKey[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
const wchar_t kEnvironment[] = L"Environment";
const unsigned int kBroadcastTimeoutMilliseconds = 2 * 1000;

}  // namespace

// FileLogger::ScopedSystemEnvironmentVariable implementation.

FileLogger::ScopedSystemEnvironmentVariable::ScopedSystemEnvironmentVariable(
    const string16& variable,
    const string16& value) {

  // Set the value in this process and its children.
  ::SetEnvironmentVariable(variable.c_str(), value.c_str());

  // Set the value for the whole system and ask everyone to refresh.
  base::win::RegKey environment;
  LONG result = environment.Open(kEnvironmentRoot, kEnvironmentKey,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS) {
    string16 old_value;
    // The actual value of the variable is insignificant in the eyes of Chrome.
    if (environment.ReadValue(variable.c_str(),
                              &old_value) != ERROR_SUCCESS &&
        environment.WriteValue(variable.c_str(),
                               value.c_str()) == ERROR_SUCCESS) {
      environment.Close();
      // Remember that this needs to be reversed in the dtor.
      variable_ = variable;
      NotifyOtherProcesses();
    }
  } else {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM to check/modify the system environment";
  }
}

FileLogger::ScopedSystemEnvironmentVariable::~ScopedSystemEnvironmentVariable()
{
  if (!variable_.empty()) {
    base::win::RegKey environment;
    if (environment.Open(kEnvironmentRoot, kEnvironmentKey,
                         KEY_SET_VALUE) == ERROR_SUCCESS) {
      environment.DeleteValue(variable_.c_str());
      environment.Close();
      NotifyOtherProcesses();
    }
  }
}

// static
void FileLogger::ScopedSystemEnvironmentVariable::NotifyOtherProcesses() {
  // Announce to the system that a change has been made so that the shell and
  // other Windowsy bits pick it up; see
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms682653.aspx.
  SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                     reinterpret_cast<LPARAM>(kEnvironment), SMTO_ABORTIFHUNG,
                     kBroadcastTimeoutMilliseconds, NULL);
}

bool FileLogger::is_initialized_ = false;

FileLogger::FileLogger()
    : event_provider_mask_() {
}

FileLogger::~FileLogger() {
  if (is_logging()) {
    LOG(ERROR)
      << __FUNCTION__ << " don't forget to call FileLogger::StopLogging()";
    StopLogging();
  }

  is_initialized_ = false;
}

// Returns false if all providers could not be enabled.  A log message is
// produced for each provider that could not be enabled.
bool FileLogger::EnableProviders() {
  // Default to false if there's at least one provider.
  bool result = (event_provider_mask_ == 0);

  // Generate ETW log events for this test binary.  Log messages at and above
  // logging::GetMinLogLevel() will continue to go to stderr as well.  This
  // leads to double logging in case of test failures: each LOG statement at
  // or above the min level will go to stderr during test execution, and then
  // all events logged to the file session will be dumped again.  If this
  // turns out to be an issue, one could call logging::SetMinLogLevel(INT_MAX)
  // here (stashing away the previous min log level to be restored in
  // DisableProviders) to suppress stderr logging during test execution.  Then
  // those events in the file that were logged at/above the old min level from
  // the test binary could be dumped to stderr if there were no failures.
  if (event_provider_mask_ & CHROME_TESTS_LOG_PROVIDER)
    logging::LogEventProvider::Initialize(kChromeTestsProvider);

  HRESULT hr = S_OK;
  for (size_t i = 0; i < arraysize(kProviders); ++i) {
    if (event_provider_mask_ & (1 << i)) {
      hr = controller_.EnableProvider(*kProviders[i].provider_name,
                                      kProviders[i].level,
                                      kProviders[i].flags);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to enable event provider " << i
                   << "; hr=" << std::hex << hr;
      } else {
        result = true;
      }
    }
  }

  return result;
}

void FileLogger::DisableProviders() {
  HRESULT hr = S_OK;
  for (size_t i = 0; i < arraysize(kProviders); ++i) {
    if (event_provider_mask_ & (1 << i)) {
      hr = controller_.DisableProvider(*kProviders[i].provider_name);
      LOG_IF(ERROR, FAILED(hr)) << "Failed to disable event provider "
                                << i << "; hr=" << std::hex << hr;
    }
  }

  if (event_provider_mask_ & CHROME_TESTS_LOG_PROVIDER)
    logging::LogEventProvider::Uninitialize();
}

void FileLogger::Initialize() {
  Initialize(kAllEventProviders);
}

void FileLogger::Initialize(uint32 event_provider_mask) {
  CHECK(!is_initialized_);

  // Set up CHROME_ETW_LOGGING in the environment if providers that require it
  // are enabled.
  if (event_provider_mask & kChromeLogProviders) {
    etw_logging_configurator_.reset(new ScopedSystemEnvironmentVariable(
        ASCIIToWide(env_vars::kEtwLogging), L"1"));
  }

  // Stop a previous session that wasn't shut down properly.
  base::win::EtwTraceProperties ignore;
  HRESULT hr = base::win::EtwTraceController::Stop(kChromeTestSession,
                                                   &ignore);
  LOG_IF(ERROR, FAILED(hr) &&
             hr != HRESULT_FROM_WIN32(ERROR_WMI_INSTANCE_NOT_FOUND))
      << "Failed to stop a previous trace session; hr=" << std::hex << hr;

  event_provider_mask_ = event_provider_mask;

  is_initialized_ = true;
}

bool FileLogger::StartLogging(const FilePath& log_file) {
  HRESULT hr =
      controller_.StartFileSession(kChromeTestSession,
                                   log_file.value().c_str(), false);
  if (SUCCEEDED(hr)) {
    // Ignore the return value here in the hopes that at least one provider was
    // enabled.
    if (!EnableProviders()) {
      LOG(ERROR) << "Failed to enable any provider.";
      controller_.Stop(NULL);
      return false;
    }
  } else {
    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
      LOG(WARNING) << "Access denied while trying to start trace session. "
                      "This is expected when not running as an administrator.";
    } else {
      LOG(ERROR) << "Failed to start trace session to file " << log_file.value()
                 << "; hr=" << std::hex << hr;
    }
    return false;
  }
  return true;
}

void FileLogger::StopLogging() {
  HRESULT hr = S_OK;

  DisableProviders();

  hr = controller_.Flush(NULL);
  LOG_IF(ERROR, FAILED(hr))
      << "Failed to flush events; hr=" << std::hex << hr;
  hr = controller_.Stop(NULL);
  LOG_IF(ERROR, FAILED(hr))
      << "Failed to stop ETW session; hr=" << std::hex << hr;
}

}  // namespace logging_win
