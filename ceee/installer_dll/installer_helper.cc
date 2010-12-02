// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Small DLL shim to register and unregister all CEEE components.

#include "ceee/installer_dll/installer_helper.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/win/registry.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/install_utils.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "chrome/common/zip.h"
#include "chrome/installer/mini_installer/pe_resource.h"
#include "chrome/installer/util/google_update_constants.h"

class InstallerHelperModule : public CAtlDllModuleT<InstallerHelperModule> {
 public:
  InstallerHelperModule();

 private:
  static const wchar_t* kLogFileName;
  base::AtExitManager at_exit_;
};

const wchar_t* InstallerHelperModule::kLogFileName = L"ff_installer.log";

InstallerHelperModule::InstallerHelperModule() {
  wchar_t logfile_path[MAX_PATH];
  DWORD len = ::GetTempPath(arraysize(logfile_path), logfile_path);
  ::PathAppend(logfile_path, kLogFileName);

  // It seems we're obliged to initialize the current command line
  // before initializing logging. This feels a little strange for
  // a DLL.
  CommandLine::Init(0, NULL);

  logging::InitLogging(
      logfile_path,
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE);
}

InstallerHelperModule _AtlModule;

namespace {

const wchar_t* kChromeFrameDllName = L"npchrome_frame.dll";
const wchar_t* kFfCeeeBaseName = L"ceee_ff";
const wchar_t* kFfCeeeExtension = L".xpi";

const wchar_t* kCeeeFirefoxExtensionName = L"ceee@google.com";

// The type of the resource we use to embed our XPI.
const wchar_t* kEmbeddedResourceType = L"BINARY";

HRESULT CallDllEntryPoint(const wchar_t* dll_name,
                          const char* function_name) {
  HRESULT hr = STG_E_FILENOTFOUND;
  FilePath path;
  PathService::Get(base::FILE_MODULE, &path);
  path = path.DirName().Append(dll_name);

  if (file_util::PathExists(path)) {
    HMODULE module = ::LoadLibrary(path.value().c_str());
    if (module) {
      FARPROC proc = ::GetProcAddress(module, function_name);
      if (proc) {
        hr = proc();
      } else {
        hr = E_INVALIDARG;
      }

      ::FreeLibrary(module);
    }
  }

  return hr;
}

// Updates @p combined_result to be @p hr but only if @p combined_result
// does not already reflect an error.
void AggregateComError(HRESULT hr, HRESULT* combined_result) {
  if (SUCCEEDED(*combined_result)) {
    *combined_result = hr;
  }
}

HRESULT RegisterIeBroker(bool do_register) {
  HRESULT hr = STG_E_FILENOTFOUND;
  FilePath broker_path;
  PathService::Get(base::FILE_MODULE, &broker_path);
  broker_path = broker_path.DirName().Append(
      ceee_module_util::kCeeeBrokerModuleName);

  if (file_util::PathExists(broker_path)) {
    CommandLine command_line(broker_path);
    command_line.AppendSwitch(do_register ? "/RegServer" : "/UnregServer");

    if (base::LaunchApp(command_line,
                        true,  // wait for process to finish
                        true,  // hide when run
                        NULL)) {
      hr = S_OK;
    } else {
      hr = com::AlwaysErrorFromLastError();
    }
  }

  return hr;
}

// Gets the path in which to install the FF CEEE.  Its important that this
// path not contain any component that is dependent on the version, otherwise
// Firefox will believe that the add-on was uninstalled and re-installed.
FilePath GetFirefoxCeeePath() {
  FilePath dir_path;
  PathService::Get(base::FILE_MODULE, &dir_path);
  return dir_path.DirName().DirName().Append(kFfCeeeBaseName);
}

// If @p extracting is true, extracts the Firefox CEEE XPI from a resource
// in @p module to disk as a sibling of the current DLL, and unzips it to a
// directory in the parent of this DLL's directory.  For example, this DLL
// typically lives at:
//
//   C:\Program Files\Google\CF\Application\<version>\ceee_installer_helper.dll
//
// where <version> is the version string of the CEEE in dotted notation, like
// 7.0.515.0.  This function extracts the resource from this DLL and puts it
// into an XPI file called:
//
//   C:\Program Files\Google\CF\Application\<version>\ceee_ff.xpi
//
// The XPI file is then unzipped into the parent directory like this:
//
//   C:\Program Files\Google\CF\Application\ceee_ff\...
//
// If @p extracting is false, deletes said XPI and directory from disk.
//
// @return S_OK if successful.
HRESULT ExtractOrDeleteFirefoxCeee(HMODULE module, bool extracting) {
  DCHECK(module);

  FilePath dir_path(GetFirefoxCeeePath());

  std::wstring xpi_file_name(kFfCeeeBaseName);
  xpi_file_name = xpi_file_name + kFfCeeeExtension;

  FilePath xpi_path;
  PathService::Get(base::FILE_MODULE, &xpi_path);
  xpi_path = xpi_path.DirName().Append(xpi_file_name);

  if (extracting) {
    // Load our XPI resource.
    PEResource resource(xpi_file_name.c_str(), kEmbeddedResourceType,
                        module);
    if (!resource.IsValid()) {
      NOTREACHED() << "Failed to find XPI resource. " << com::LogWe();
      return com::AlwaysErrorFromLastError();
    }

    // Write the XPI to disk.
    bool success = resource.WriteToDisk(xpi_path.value().c_str());
    if (!success) {
      NOTREACHED() << "Failed to write XPI to disk. " << com::LogWe();
      return com::AlwaysErrorFromLastError();
    }

    // Create the directory to hold the unzipped contents.
    success = file_util::CreateDirectory(dir_path);
    if (!success) {
      NOTREACHED() << "Failed to create destination directory. "
          << com::LogWe();
      return com::AlwaysErrorFromLastError();
    }

    // Unzip it.
    success = Unzip(xpi_path, dir_path);
    if (!success) {
      NOTREACHED() << "Failed to unpack archive. " << com::LogWe();
      return com::AlwaysErrorFromLastError();
    }

    return S_OK;
  } else {
    HRESULT hr = S_OK;
    if (file_util::PathExists(xpi_path) &&
        !file_util::Delete(xpi_path, false)) {
      NOTREACHED() << "Failed to delete XPI." << com::LogWe();
      hr = com::AlwaysErrorFromLastError();
    }

    // Try deleting directory even if deleting XPI failed.
    // This is kind of dangerous (e.g. with an empty filepath) so make
    // an assertion that the basename is ceee_ff (we know this to be true,
    // this is in case somebody breaks the code above), meaning we're not
    // about to do an rm -rf on the root of your file system.
    if (dir_path.BaseName().value() == kFfCeeeBaseName) {
      if (file_util::PathExists(dir_path) &&
          !file_util::Delete(dir_path, true)) {
        NOTREACHED() << "Failed to delete XPI." << com::LogWe();
        hr = com::AlwaysErrorFromLastError();
      }
    } else {
      NOTREACHED() << "You were going to delete some other directory?";
      hr = E_UNEXPECTED;
    }

    return hr;
  }
}

HRESULT RegisterFirefoxCeee(bool do_register) {
  HRESULT hr = STG_E_FILENOTFOUND;
  if (do_register) {
    FilePath path(GetFirefoxCeeePath());

    if (file_util::PathExists(path)) {
      base::win::RegKey key(HKEY_LOCAL_MACHINE, L"SOFTWARE", KEY_WRITE);
      if (key.CreateKey(L"Mozilla", KEY_WRITE) &&
          key.CreateKey(L"Firefox", KEY_WRITE) &&
          key.CreateKey(L"extensions", KEY_WRITE) &&
          key.WriteValue(kCeeeFirefoxExtensionName, path.value().c_str())) {
        hr = S_OK;
      } else {
        hr = com::AlwaysErrorFromLastError();
      }
    }
  } else {
    hr = S_OK;  // OK if not found, then there's nothing to do
    base::win::RegKey key(HKEY_LOCAL_MACHINE, L"SOFTWARE", KEY_READ);
    if (key.OpenKey(L"Mozilla", KEY_READ) &&
        key.OpenKey(L"Firefox", KEY_READ) &&
        key.OpenKey(L"extensions", KEY_WRITE) &&
        key.ValueExists(kCeeeFirefoxExtensionName)) {
      if (!key.DeleteValue(kCeeeFirefoxExtensionName)) {
        hr = com::AlwaysErrorFromLastError();
      }
    }
  }

  return hr;
}

const wchar_t kCeeeChannelTag[] = L"-CEEE";

const HKEY reg_root = HKEY_LOCAL_MACHINE;

// Registers this install as coming from the CEEE+CF channel if it was
// installed using Omaha.
HRESULT RegisterCeeeChannel() {
  std::wstring reg_key(ceee_module_util::GetCromeFrameClientStateKey());

  base::win::RegKey key;
  if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS)) {
    // Omaha didn't install the key.  Perhaps no Omaha?  That's ok.
    return S_OK;
  }

  std::wstring ap_key_value;
  if (!key.ReadValue(google_update::kRegApField, &ap_key_value)) {
    // Key doesn't exist yet.
    if (!key.WriteValue(google_update::kRegApField, kCeeeChannelTag)) {
      return E_FAIL;
    }
    return S_OK;
  }

  if (ap_key_value.find(kCeeeChannelTag) == std::wstring::npos) {
    // Key doesn't contain -CEEE
    ap_key_value.append(kCeeeChannelTag);
    if (!key.WriteValue(google_update::kRegApField, ap_key_value.c_str())) {
      return E_FAIL;
    }
    return S_OK;
  }

  // Everything we need is already done!
  return S_OK;
}

// Removes any registration information written by RegisterCeeeChannel.
HRESULT UnregisterCeeeChannel() {
  std::wstring reg_key(ceee_module_util::GetCromeFrameClientStateKey());
  base::win::RegKey key;
  if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS)) {
    // Omaha didn't install the key.
    return S_OK;
  }

  std::wstring ap_key_value;
  if (!key.ReadValue(google_update::kRegApField, &ap_key_value)) {
    // Key doesn't exist.  Nothing to do.
    return S_OK;
  }

  size_t pos = ap_key_value.find(kCeeeChannelTag);

  if (pos == std::wstring::npos) {
    // Key doesn't contain -CEEE.  Nothing to do.
    return S_OK;
  }

  // Prune -CEEE from ap and write it.
  ap_key_value.erase(pos, wcslen(kCeeeChannelTag));
  if (!key.WriteValue(google_update::kRegApField, ap_key_value.c_str())) {
    return E_FAIL;
  }
  return S_OK;
}
}  // namespace


// DLL Entry Point.
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  return _AtlModule.DllMain(reason, reserved);
}

// Used to determine whether the DLL can be unloaded by OLE.
STDAPI DllCanUnloadNow(void) {
  return _AtlModule.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type.
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

// Adds entries to the system registry.
//
// This is not the actual entrypoint; see the macro right below this
// function, which keeps us safe from ever forgetting to check for
// the --enable-ceee flag.
STDAPI DllRegisterServerImpl(void) {
  HRESULT hr = _AtlModule.DllRegisterServer(FALSE);

  // This function assumes the DLL is being registered with admin privileges.

  // In case of error, we abort the rest of registration - we're in bad
  // shape anyway.
  if (SUCCEEDED(hr)) {
    hr = RegisterIeBroker(true);
    DCHECK(SUCCEEDED(hr)) << "Could not register CEEE IE Broker" <<
        com::LogHr(hr);
  }

  // We only check for the --enable-ff-ceee flag here, and not at
  // unregistration time; when unregistering, the Firefox-related
  // unregistration code is written such that it will not give an
  // error when no unregistration was necessary, so it's safe to
  // always try.
  if (ceee_install_utils::ShouldRegisterFfCeee()) {
    if (SUCCEEDED(hr)) {
      hr = CallDllEntryPoint(kChromeFrameDllName, "RegisterNPAPIPlugin");
      DCHECK(SUCCEEDED(hr)) << "Could not register CF with FF" <<
          com::LogHr(hr);
    }

    if (SUCCEEDED(hr)) {
      hr = ExtractOrDeleteFirefoxCeee(_AtlBaseModule.GetModuleInstance(),
                                      true);
      DCHECK(SUCCEEDED(hr)) << "Unable to extract CEEE for FF" <<
          com::LogHr(hr);
    }

    if (SUCCEEDED(hr)) {
      hr = RegisterFirefoxCeee(true);
      DCHECK(SUCCEEDED(hr)) << "Could not register CEEE with FF"
          << com::LogHr(hr);
    }
  }

  if (SUCCEEDED(hr)) {
    hr = RegisterCeeeChannel();
    DCHECK(SUCCEEDED(hr)) << "Could not register with Omaha for CEEE+CF channel"
        << com::LogHr(hr);
  }

  return hr;
}

CEEE_DEFINE_DLL_REGISTER_SERVER()

// Removes entries from the system registry.
STDAPI DllUnregisterServer(void) {
  // We always allow unregistration, even if no --enable-ceee install flag.
  //
  // We also always unregister Firefox components, regardless of whether the
  // --enable-ff-ceee flag is set; unregistration is idempotent.

  HRESULT combined_result = S_OK;
  HRESULT hr = _AtlModule.DllUnregisterServer(FALSE);
  AggregateComError(hr, &combined_result);

  // This function assumes the DLL is being unregistered with admin privileges.

  // In case of error, we continue with the rest of unregistration, to
  // clean up as much as possible.
  hr = RegisterIeBroker(false);
  DCHECK(SUCCEEDED(hr)) << "Could not unregister CEEE IE Broker" <<
      com::LogHr(hr);
  AggregateComError(hr, &combined_result);

  hr = CallDllEntryPoint(kChromeFrameDllName, "UnregisterNPAPIPlugin");
  DCHECK(SUCCEEDED(hr)) << "Could not unregister CF with FF" << com::LogHr(hr);
  AggregateComError(hr, &combined_result);

  hr = RegisterFirefoxCeee(false);
  DCHECK(SUCCEEDED(hr)) << "Could not unregister CEEE with FF" <<
      com::LogHr(hr);
  AggregateComError(hr, &combined_result);

  hr = ExtractOrDeleteFirefoxCeee(_AtlBaseModule.GetModuleInstance(), false);
  DCHECK(SUCCEEDED(hr)) << "Unable to extract CEEE for FF" << com::LogHr(hr);
  AggregateComError(hr, &combined_result);

  hr = UnregisterCeeeChannel();
  DCHECK(SUCCEEDED(hr)) << "Could not unregister with Omaha for corp channel"
    << com::LogHr(hr);
  AggregateComError(hr, &combined_result);

  return combined_result;
}

// No-op entry point to keep user-level registration happy.
// TODO(robertshield): Remove this as part of registration re-org.
STDAPI DllRegisterUserServer() {
  LOG(WARNING) << "Call to unimplemented DllRegisterUserServer.";
  return S_OK;
}

// No-op entry point to keep user-level unregistration happy.
// TODO(robertshield): Remove this as part of registration re-org.
STDAPI DllUnregisterUserServer() {
  LOG(WARNING) << "Call to unimplemented DllUnregisterUserServer.";
  return S_OK;
}
