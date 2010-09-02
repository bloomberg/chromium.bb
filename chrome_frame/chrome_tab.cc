// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome_tab.cc : Implementation of DLL Exports.

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

#include <atlsecurity.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chrome_frame_resources.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/chrome_active_document.h"
#include "chrome_frame/chrome_frame_activex.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/chrome_frame_reporting.h"
#include "chrome_frame/chrome_launcher_utils.h"
#include "chrome_frame/chrome_protocol.h"
#include "chrome_frame/module_utils.h"
#include "chrome_frame/resource.h"
#include "chrome_frame/utils.h"
#include "googleurl/src/url_util.h"

namespace {
// This function has the side effect of initializing an unprotected
// vector pointer inside GoogleUrl. If this is called during DLL loading,
// it has the effect of avoiding an initialization race on that pointer.
// TODO(siggi): fix GoogleUrl.
void InitGoogleUrl() {
  static const char kDummyUrl[] = "http://www.google.com";

  url_util::IsStandard(kDummyUrl,
                       url_parse::MakeRange(0, arraysize(kDummyUrl)));
}
}

static const wchar_t kBhoRegistryPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
    L"\\Browser Helper Objects";

const wchar_t kInternetSettings[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

const wchar_t kProtocolHandlers[] =
    L"Software\\Classes\\Protocols\\Handler";

const wchar_t kRunOnce[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";

const wchar_t kBhoNoLoadExplorerValue[] = L"NoExplorer";

// {0562BFC3-2550-45b4-BD8E-A310583D3A6F}
static const GUID kChromeFrameProvider =
    { 0x562bfc3, 0x2550, 0x45b4,
        { 0xbd, 0x8e, 0xa3, 0x10, 0x58, 0x3d, 0x3a, 0x6f } };

// Object entries go here instead of with each object, so that we can move
// the objects to a lib. Also reduces magic.
OBJECT_ENTRY_AUTO(CLSID_ChromeFrameBHO, Bho)
OBJECT_ENTRY_AUTO(__uuidof(ChromeActiveDocument), ChromeActiveDocument)
OBJECT_ENTRY_AUTO(__uuidof(ChromeFrame), ChromeFrameActivex)
OBJECT_ENTRY_AUTO(__uuidof(ChromeProtocol), ChromeProtocol)


// See comments in DllGetClassObject.
LPFNGETCLASSOBJECT g_dll_get_class_object_redir_ptr = NULL;

class ChromeTabModule : public CAtlDllModuleT<ChromeTabModule> {
 public:
  typedef CAtlDllModuleT<ChromeTabModule> ParentClass;

  ChromeTabModule() : do_system_registration_(true) {}

  DECLARE_LIBID(LIBID_ChromeTabLib)
  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CHROMETAB,
                                    "{FD9B1B31-F4D8-436A-8F4F-D3C2E36733D3}")

  // Override to add our SYSTIME binary value to registry scripts.
  // See chrome_frame_activex.rgs for usage.
  virtual HRESULT AddCommonRGSReplacements(IRegistrarBase* registrar) throw() {
    HRESULT hr = ParentClass::AddCommonRGSReplacements(registrar);

    if (SUCCEEDED(hr)) {
      SYSTEMTIME local_time;
      ::GetSystemTime(&local_time);
      std::string hex(base::HexEncode(&local_time, sizeof(local_time)));
      base::StringPiece sp_hex(hex);
      hr = registrar->AddReplacement(L"SYSTIME",
                                     base::SysNativeMBToWide(sp_hex).c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      FilePath app_path =
          chrome_launcher::GetChromeExecutablePath().DirName();
      hr = registrar->AddReplacement(L"CHROME_APPPATH",
                                     app_path.value().c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      hr = registrar->AddReplacement(L"CHROME_APPNAME",
                                     chrome::kBrowserProcessExecutableName);
      DCHECK(SUCCEEDED(hr));

      // Fill in VERSION from the VERSIONINFO stored in the DLL's resources.
      scoped_ptr<FileVersionInfo> module_version_info(
          FileVersionInfo::CreateFileVersionInfoForCurrentModule());
      DCHECK(module_version_info != NULL);
      std::wstring file_version(module_version_info->file_version());
      hr = registrar->AddReplacement(L"VERSION", file_version.c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      // Add the directory of chrome_launcher.exe.  This will be the same
      // as the directory for the current DLL.
      std::wstring module_dir;
      FilePath module_path;
      if (PathService::Get(base::FILE_MODULE, &module_path)) {
        module_dir = module_path.DirName().value();
      } else {
        NOTREACHED();
      }
      hr = registrar->AddReplacement(L"CHROME_LAUNCHER_APPPATH",
                                     module_dir.c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      // Add the filename of chrome_launcher.exe
      hr = registrar->AddReplacement(L"CHROME_LAUNCHER_APPNAME",
                                     chrome_launcher::kLauncherExeBaseName);
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      // Add the registry hive to use.
      // Note: This is ugly as hell. I'd rather use the pMapEntries parameter
      // to CAtlModule::UpdateRegistryFromResource, unfortunately we have a
      // few components that are registered by calling their
      // static T::UpdateRegistry() methods directly, which doesn't allow
      // pMapEntries to be passed through :-(
      if (do_system_registration_) {
        hr = registrar->AddReplacement(L"HIVE", L"HKLM");
      } else {
        hr = registrar->AddReplacement(L"HIVE", L"HKCU");
      }
      DCHECK(SUCCEEDED(hr));
    }

    return hr;
  }

  // See comments in AddCommonRGSReplacements
  bool do_system_registration_;
};

ChromeTabModule _AtlModule;

base::AtExitManager* g_exit_manager = NULL;
bool RegisterSecuredMimeHandler(bool enable, bool is_system);  // forward

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  UNREFERENCED_PARAMETER(instance);
  if (reason == DLL_PROCESS_ATTACH) {
#ifndef NDEBUG
    // Silence traces from the ATL registrar to reduce the log noise.
    ATL::CTrace::s_trace.ChangeCategory(atlTraceRegistrar, 0,
                                        ATLTRACESTATUS_DISABLED);
#endif
    InitGoogleUrl();

    g_exit_manager = new base::AtExitManager();
    CommandLine::Init(0, NULL);
    InitializeCrashReporting();
    logging::InitLogging(NULL, logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                        logging::LOCK_LOG_FILE, logging::DELETE_OLD_LOG_FILE);

    if (!DllRedirector::RegisterAsFirstCFModule()) {
      // We are not the first ones in, get the module who registered first.
      HMODULE original_module = DllRedirector::GetFirstCFModule();
      DCHECK(original_module != NULL)
          << "Could not get first CF module handle.";
      HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
      if (original_module != this_module) {
        // Someone else was here first, try and get a pointer to their
        // DllGetClassObject export:
        g_dll_get_class_object_redir_ptr =
            DllRedirector::GetDllGetClassObjectPtr(original_module);
        DCHECK(g_dll_get_class_object_redir_ptr != NULL)
            << "Found CF module with no DllGetClassObject export.";
      }
    }

    // Enable ETW logging.
    logging::LogEventProvider::Initialize(kChromeFrameProvider);
  } else if (reason == DLL_PROCESS_DETACH) {
    DllRedirector::UnregisterAsFirstCFModule();
    g_patch_helper.UnpatchIfNeeded();
    delete g_exit_manager;
    g_exit_manager = NULL;
    ShutdownCrashReporting();
  }
  return _AtlModule.DllMain(reason, reserved);
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

HRESULT RefreshElevationPolicy() {
  const wchar_t kIEFrameDll[] = L"ieframe.dll";
  const char kIERefreshPolicy[] = "IERefreshElevationPolicy";
  HRESULT hr = E_NOTIMPL;

  // Stick an SEH in the chain to prevent the VEH from picking up on first
  // chance exceptions caused by loading ieframe.dll. Use the vanilla
  // ExceptionBarrier to report any exceptions that do make their way to us
  // though.
  ExceptionBarrier barrier;

  HMODULE ieframe_module = LoadLibrary(kIEFrameDll);
  if (ieframe_module) {
    typedef HRESULT (__stdcall *IERefreshPolicy)();
    IERefreshPolicy ie_refresh_policy = reinterpret_cast<IERefreshPolicy>(
        GetProcAddress(ieframe_module, kIERefreshPolicy));

    if (ie_refresh_policy) {
      hr = ie_refresh_policy();
    } else {
      hr = HRESULT_FROM_WIN32(GetLastError());
    }

    FreeLibrary(ieframe_module);
  } else {
    hr = HRESULT_FROM_WIN32(GetLastError());
  }

  return hr;
}

// Experimental boot prefetch optimization for Chrome Frame
//
// If chrome is warmed up during a single reboot, it gets paged
// in for subsequent reboots and the cold startup times essentially
// look like warm times thereafter! The 'warm up' is done by
// setting up a 'RunOnce' key during DLLRegisterServer of
// npchrome_frame.dll.
//
// This works because chrome prefetch becomes part of boot
// prefetch file ntosboot-b00dfaad.pf and paged in on subsequent
// reboots. As long as the sytem does not undergo significant
// memory pressure those pages remain in memory and we get pretty
// amazing startup times, down to about 300 ms from 1200 ms
//
// The downside is:
// - Whether chrome frame is used or not, there's a read penalty
//  (1200-300 =) 900 ms for every boot.
// - Heavy system memory usage after reboot will nullify the benefits
//  but the user will still pay the cost.
// - Overall the time saved will always be less than total time spent
//  paging in chrome
// - We are not sure when the chrome 'warm up' will age out from the
//  boot prefetch file.
//
// The idea here is to try this out on chrome frame dev channel
// and see if it produces a significant drift in startup numbers.
HRESULT SetupRunOnce() {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA)
    return S_OK;

  std::wstring channel_name;
  if (!GoogleUpdateSettings::GetChromeChannel(true, &channel_name) ||
      (0 != lstrcmpiW(L"dev", channel_name.c_str()))) {
    return S_OK;
  }

  RegKey run_once;
  if (run_once.Create(HKEY_CURRENT_USER, kRunOnce, KEY_READ | KEY_WRITE)) {
    CommandLine run_once_command(chrome_launcher::GetChromeExecutablePath());
    run_once_command.AppendSwitchASCII(switches::kAutomationClientChannelID,
                                       "0");
    run_once_command.AppendSwitch(switches::kChromeFrame);
    run_once.WriteValue(L"A", run_once_command.command_line_string().c_str());
  }

  return S_OK;
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow() {
  return _AtlModule.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  // If we found another module present when we were loaded, then delegate to
  // that:
  if (g_dll_get_class_object_redir_ptr) {
    return g_dll_get_class_object_redir_ptr(rclsid, riid, ppv);
  }

  // Enable sniffing and switching only if asked for BHO
  // (we use BHO to get loaded in IE).
  if (rclsid == CLSID_ChromeFrameBHO) {
    g_patch_helper.InitializeAndPatchProtocolsIfNeeded();
  }

  return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

enum RegistrationFlags {
  ACTIVEX             = 0x0001,
  ACTIVEDOC           = 0x0002,
  GCF_PROTOCOL        = 0x0004,
  BHO_CLSID           = 0x0008,
  BHO_REGISTRATION    = 0x0010,
  TYPELIB             = 0x0020,

  NPAPI_PLUGIN        = 0x1000,

  ALL                 = 0xFFFF
};

STDAPI CustomRegistration(UINT reg_flags, BOOL reg, bool is_system) {
  UINT flags = reg_flags;

  if (reg && (flags & (ACTIVEDOC | ACTIVEX)))
    flags |= (TYPELIB |GCF_PROTOCOL);

  HRESULT hr = S_OK;

  // Set the flag that gets checked in AddCommonRGSReplacements before doing
  // registration work.
  _AtlModule.do_system_registration_ = is_system;

  if ((hr == S_OK) && (flags & ACTIVEDOC)) {
    // Don't fail to unregister if we can't undo the secure mime
    // handler registration. This was observed getting hit during
    // uninstallation.
    if (!RegisterSecuredMimeHandler(reg ? true : false, is_system) && reg)
      return E_FAIL;
    hr = ChromeActiveDocument::UpdateRegistry(reg);
  }

  if ((hr == S_OK) && (flags & ACTIVEX)) {
    // We have to call the static T::UpdateRegistry function instead of
    // _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_ACTIVEX, reg)
    // because there is specific OLEMISC replacement.
    hr = ChromeFrameActivex::UpdateRegistry(reg);
    // TODO(amit): Move elevation policy registration from ActiveX rgs
    // into a separate rgs.
    RefreshElevationPolicy();
  }

  if ((hr == S_OK) && (flags & GCF_PROTOCOL)) {
    hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEPROTOCOL, reg);
  }

  if ((hr == S_OK) && (flags & BHO_CLSID)) {
    hr = Bho::UpdateRegistry(reg);
  }

  if ((hr == S_OK) && (flags & BHO_REGISTRATION) && is_system) {
    _AtlModule.UpdateRegistryFromResourceS(IDR_REGISTER_BHO, reg);
  }

  if ((hr == S_OK) && (flags & TYPELIB)) {
    hr = (reg)?
        UtilRegisterTypeLib(_AtlComModule.m_hInstTypeLib, NULL, !is_system) :
        UtilUnRegisterTypeLib(_AtlComModule.m_hInstTypeLib, NULL, !is_system);
  }

  if ((hr == S_OK) && (flags & NPAPI_PLUGIN)) {
    hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI, reg);
  }

  if (hr == S_OK) {
    hr = _AtlModule.UpdateRegistryAppId(reg);
  }

  return hr;
}



// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer() {
  UINT flags =  ACTIVEX | ACTIVEDOC | TYPELIB | GCF_PROTOCOL |
                BHO_CLSID | BHO_REGISTRATION;

  if (UtilIsPersistentNPAPIMarkerSet()) {
    flags |= IDR_CHROMEFRAME_NPAPI;
  }

  HRESULT hr = CustomRegistration(flags, TRUE, true);
  if (SUCCEEDED(hr)) {
    SetupRunOnce();
  }

  return hr;
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer() {
  HRESULT hr = CustomRegistration(ALL, FALSE, true);
  return hr;
}

// DllRegisterServer - Adds entries to the HKCU hive in the registry
STDAPI DllRegisterUserServer() {
  UINT flags =  ACTIVEX | ACTIVEDOC | TYPELIB | GCF_PROTOCOL | BHO_CLSID;

  if (UtilIsPersistentNPAPIMarkerSet()) {
    flags |= IDR_CHROMEFRAME_NPAPI;
  }

  HRESULT hr = CustomRegistration(flags, TRUE, false);
  if (SUCCEEDED(hr)) {
    SetupRunOnce();
  }

  return hr;
}

// DllRegisterServer - Removes entries from the HKCU hive in the registry.
STDAPI DllUnregisterUserServer() {
  HRESULT hr = CustomRegistration(ALL, FALSE, false);
  return hr;
}

// Registers the NPAPI plugin and sets the persistent marker that tells us
// to re-register it through updates.
STDAPI RegisterNPAPIPlugin() {
  HRESULT hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI,
                                                      TRUE);
  if (SUCCEEDED(hr)) {
    if (!UtilChangePersistentNPAPIMarker(true)) {
      hr = E_FAIL;
    }
  }
  return hr;
}

// Unregisters the NPAPI plugin and clears the persistent marker that tells us
// to re-register it through updates.
STDAPI UnregisterNPAPIPlugin() {
  HRESULT hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI,
                                                      FALSE);
  if (SUCCEEDED(hr)) {
    if (!UtilChangePersistentNPAPIMarker(false)) {
      hr = E_FAIL;
    }
  }
  return hr;
}

class SecurityDescBackup {
 public:
  explicit SecurityDescBackup(const std::wstring& backup_key)
      : backup_key_name_(backup_key) {}
  ~SecurityDescBackup() {}

  // Save given security descriptor to the backup key.
  bool SaveSecurity(const CSecurityDesc& sd) {
    CString str;
    if (!sd.ToString(&str))
      return false;

    RegKey backup_key(HKEY_LOCAL_MACHINE, backup_key_name_.c_str(),
                      KEY_READ | KEY_WRITE);
    if (backup_key.Valid()) {
      return backup_key.WriteValue(NULL, str.GetString());
    }

    return false;
  }

  // Restore security descriptor from backup key to given key name.
  bool RestoreSecurity(const wchar_t* key_name) {
    std::wstring sddl;
    if (!ReadBackupKey(&sddl))
      return false;

    // Create security descriptor from string.
    CSecurityDesc sd;
    if (!sd.FromString(sddl.c_str()))
      return false;

    bool result = true;
    // Restore DACL and Owner of the key from saved security descriptor.
    CDacl dacl;
    CSid owner;
    sd.GetDacl(&dacl);
    sd.GetOwner(&owner);

    DWORD error = ::SetNamedSecurityInfo(const_cast<wchar_t*>(key_name),
        SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        const_cast<SID*>(owner.GetPSID()), NULL,
        const_cast<ACL*>(dacl.GetPACL()), NULL);

    DeleteBackupKey();
    return (error == ERROR_SUCCESS);
  }

 private:
  // Read SDDL string from backup key
  bool ReadBackupKey(std::wstring* sddl) {
    RegKey backup_key(HKEY_LOCAL_MACHINE, backup_key_name_.c_str(), KEY_READ);
    if (!backup_key.Valid())
      return false;

    DWORD len = 0;
    DWORD reg_type = REG_NONE;
    if (!backup_key.ReadValue(NULL, NULL, &len, &reg_type))
      return false;

    if (reg_type != REG_SZ)
      return false;

    size_t wchar_count = 1 + len / sizeof(wchar_t);
    if (!backup_key.ReadValue(NULL, WriteInto(sddl, wchar_count), &len,
                              &reg_type)) {
      return false;
    }

    return true;
  }

  void DeleteBackupKey() {
    ::RegDeleteKey(HKEY_LOCAL_MACHINE, backup_key_name_.c_str());
  }

  std::wstring backup_key_name_;
};

struct TokenWithPrivileges {
  TokenWithPrivileges() {
    token_.GetEffectiveToken(TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY);
    token_.GetUser(&user_);
  }

  ~TokenWithPrivileges() {
    token_.EnableDisablePrivileges(take_ownership_);
    token_.EnableDisablePrivileges(restore_);
  }

  bool EnablePrivileges() {
    if (take_ownership_.GetCount() == 0)
      if (!token_.EnablePrivilege(L"SeTakeOwnershipPrivilege",
                                  &take_ownership_))
        return false;

    if (restore_.GetCount() == 0)
      if (!token_.EnablePrivilege(L"SeRestorePrivilege", &restore_))
        return false;

    return true;
  }

  const CSid& GetUser() const {
    return user_;
  }

 private:
  CAccessToken token_;
  CTokenPrivileges take_ownership_;
  CTokenPrivileges restore_;
  CSid user_;
};

static bool SetOrDeleteMimeHandlerKey(bool set, HKEY root_key) {
  std::wstring key_name = kInternetSettings;
  key_name.append(L"\\Secure Mime Handlers");
  RegKey key(root_key, key_name.c_str(), KEY_READ | KEY_WRITE);
  if (!key.Valid())
    return false;

  bool result;
  if (set) {
    result = key.WriteValue(L"ChromeTab.ChromeActiveDocument", 1);
    result = key.WriteValue(L"ChromeTab.ChromeActiveDocument.1", 1) && result;
  } else {
    result = key.DeleteValue(L"ChromeTab.ChromeActiveDocument");
    result = key.DeleteValue(L"ChromeTab.ChromeActiveDocument.1") && result;
  }

  return result;
}

bool RegisterSecuredMimeHandler(bool enable, bool is_system) {
  if (!is_system) {
    return SetOrDeleteMimeHandlerKey(enable, HKEY_CURRENT_USER);
  } else if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    return SetOrDeleteMimeHandlerKey(enable, HKEY_LOCAL_MACHINE);
  }

  std::wstring mime_key = kInternetSettings;
  mime_key.append(L"\\Secure Mime Handlers");
  std::wstring backup_key = kInternetSettings;
  backup_key.append(L"\\__backup_SMH__");
  std::wstring object_name = L"MACHINE\\";
  object_name.append(mime_key);

  TokenWithPrivileges token_;
  if (!token_.EnablePrivileges())
    return false;

  // If there is a backup key - something bad happened; try to restore
  // security on "Secure Mime Handlers" from the backup.
  SecurityDescBackup backup(backup_key);
  backup.RestoreSecurity(object_name.c_str());

  // Read old security descriptor of the Mime key first.
  CSecurityDesc sd;
  if (!AtlGetSecurityDescriptor(object_name.c_str(), SE_REGISTRY_KEY, &sd)) {
    return false;
  }

  backup.SaveSecurity(sd);
  bool result = false;
  // set new owner
  if (AtlSetOwnerSid(object_name.c_str(), SE_REGISTRY_KEY, token_.GetUser())) {
    // set new dacl
    CDacl new_dacl;
    sd.GetDacl(&new_dacl);
    new_dacl.AddAllowedAce(token_.GetUser(), GENERIC_WRITE | GENERIC_READ);
    if (AtlSetDacl(object_name.c_str(), SE_REGISTRY_KEY, new_dacl)) {
      result = SetOrDeleteMimeHandlerKey(enable, HKEY_LOCAL_MACHINE);
    }
  }

  backup.RestoreSecurity(object_name.c_str());
  return result;
}
