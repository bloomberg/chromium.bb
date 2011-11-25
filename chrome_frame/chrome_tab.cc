// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome_tab.cc : Implementation of DLL Exports.

// Need to include this before the ATL headers below.
#include "chrome_frame/chrome_tab.h"

#include <atlsecurity.h>
#include <objbase.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/chrome_active_document.h"
#include "chrome_frame/chrome_frame_activex.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_reporting.h"
#include "chrome_frame/chrome_launcher_utils.h"
#include "chrome_frame/chrome_protocol.h"
#include "chrome_frame/dll_redirector.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/resource.h"
#include "chrome_frame/utils.h"
#include "googleurl/src/url_util.h"
#include "grit/chrome_frame_resources.h"

using base::win::RegKey;

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

const wchar_t kInternetSettings[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

const wchar_t kProtocolHandlers[] =
    L"Software\\Classes\\Protocols\\Handler";

const wchar_t kRunOnce[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";

const wchar_t kRunKeyName[] = L"ChromeFrameHelper";

const wchar_t kChromeFrameHelperExe[] = L"chrome_frame_helper.exe";
const wchar_t kChromeFrameHelperStartupArg[] = L"--startup";

// Window class and window names.
// TODO(robertshield): These and other constants need to be refactored into
// a common chrome_frame_constants.h|cc and built into a separate lib
// (either chrome_frame_utils or make another one).
const wchar_t kChromeFrameHelperWindowClassName[] =
    L"ChromeFrameHelperWindowClass";
const wchar_t kChromeFrameHelperWindowName[] =
    L"ChromeFrameHelperWindowName";

// {0562BFC3-2550-45b4-BD8E-A310583D3A6F}
static const GUID kChromeFrameProvider =
    { 0x562bfc3, 0x2550, 0x45b4,
        { 0xbd, 0x8e, 0xa3, 0x10, 0x58, 0x3d, 0x3a, 0x6f } };

const wchar_t kPostPlatformUAKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\"
    L"User Agent\\Post Platform";
const wchar_t kChromeFramePrefix[] = L"chromeframe/";

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

    if (SUCCEEDED(hr)) {
      // Add the Chrome Frame CLSID.
      wchar_t cf_clsid[64];
      StringFromGUID2(CLSID_ChromeFrame, &cf_clsid[0], arraysize(cf_clsid));
      hr = registrar->AddReplacement(L"CHROME_FRAME_CLSID", &cf_clsid[0]);
    }

    return hr;
  }

  // See comments in AddCommonRGSReplacements
  bool do_system_registration_;
};

ChromeTabModule _AtlModule;

base::AtExitManager* g_exit_manager = NULL;


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
  HRESULT result = E_FAIL;

  std::wstring channel_name;
  if (base::win::GetVersion() < base::win::VERSION_VISTA &&
      GoogleUpdateSettings::GetChromeChannelAndModifiers(true, &channel_name)) {
    std::transform(channel_name.begin(), channel_name.end(),
                   channel_name.begin(), tolower);
    // Use this only for the dev channel and CEEE channels.
    if (channel_name.find(L"dev") != std::wstring::npos ||
        channel_name.find(L"ceee") != std::wstring::npos) {
      HKEY hive = HKEY_CURRENT_USER;
      if (IsSystemProcess()) {
        // For system installs, our updates will be running as SYSTEM which
        // makes writing to a RunOnce key under HKCU not so terribly useful.
        hive = HKEY_LOCAL_MACHINE;
      }

      RegKey run_once;
      LONG ret = run_once.Create(hive, kRunOnce, KEY_READ | KEY_WRITE);
      if (ret == ERROR_SUCCESS) {
        CommandLine run_once_cmd(chrome_launcher::GetChromeExecutablePath());
        run_once_cmd.AppendSwitchASCII(switches::kAutomationClientChannelID,
                                       "0");
        run_once_cmd.AppendSwitch(switches::kChromeFrame);
        ret = run_once.WriteValue(L"A",
                                  run_once_cmd.GetCommandLineString().c_str());
      }
      result = HRESULT_FROM_WIN32(ret);
    } else {
      result = S_FALSE;
    }
  } else {
    // We're on a non-XP version of Windows or on a stable channel. Nothing
    // needs doing.
    result = S_FALSE;
  }

  return result;
}

// Helper method called for user-level installs where we don't have admin
// permissions. Starts up the long running process and registers it to get it
// started at next boot.
HRESULT SetupUserLevelHelper() {
  HRESULT hr = S_OK;

  // Remove existing run-at-startup entry.
  base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER, kRunKeyName);

  // Build the chrome_frame_helper command line.
  FilePath module_path;
  FilePath helper_path;
  if (PathService::Get(base::FILE_MODULE, &module_path)) {
    module_path = module_path.DirName();
    helper_path = module_path.Append(kChromeFrameHelperExe);
    if (!file_util::PathExists(helper_path)) {
      // If we can't find the helper in the current directory, try looking
      // one up (this is the layout in the build output folder).
      module_path = module_path.DirName();
      helper_path = module_path.Append(kChromeFrameHelperExe);
      DCHECK(file_util::PathExists(helper_path)) <<
          "Could not find chrome_frame_helper.exe.";
    }

    // Find window handle of existing instance.
    HWND old_window = FindWindow(kChromeFrameHelperWindowClassName,
                                 kChromeFrameHelperWindowName);

    if (file_util::PathExists(helper_path)) {
      std::wstring helper_path_cmd(L"\"");
      helper_path_cmd += helper_path.value();
      helper_path_cmd += L"\" ";
      helper_path_cmd += kChromeFrameHelperStartupArg;

      // Add new run-at-startup entry.
      if (!base::win::AddCommandToAutoRun(HKEY_CURRENT_USER, kRunKeyName,
                                          helper_path_cmd)) {
        hr = E_FAIL;
        LOG(ERROR) << "Could not add helper process to auto run key.";
      }

      // Start new instance.
      base::LaunchOptions options;
      options.start_hidden = true;
      bool launched = base::LaunchProcess(helper_path.value(), options, NULL);
      if (!launched) {
        hr = E_FAIL;
        PLOG(DFATAL) << "Could not launch helper process.";
      }

      // Kill old instance using window handle.
      if (IsWindow(old_window)) {
        BOOL result = PostMessage(old_window, WM_CLOSE, 0, 0);
        if (!result) {
          PLOG(ERROR) << "Failed to post close message to old helper process: ";
        }
      }
    } else {
      hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
  } else {
    hr = E_UNEXPECTED;
    NOTREACHED();
  }

  return hr;
}

// To delete the user agent, set value to NULL.
// The is_system parameter indicates whether this is a per machine or a per
// user installation.
HRESULT SetChromeFrameUA(bool is_system, const wchar_t* value) {
  HRESULT hr = E_FAIL;
  HKEY parent_hive = is_system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey ua_key;
  LONG reg_result = ua_key.Create(parent_hive, kPostPlatformUAKey,
                                  KEY_READ | KEY_WRITE);
  if (reg_result == ERROR_SUCCESS) {
    // Make sure that we unregister ChromeFrame UA strings registered previously
    wchar_t value_name[MAX_PATH + 1] = {};
    wchar_t value_data[MAX_PATH + 1] = {};

    DWORD value_index = 0;
    while (value_index < ua_key.GetValueCount()) {
      DWORD name_size = arraysize(value_name);
      DWORD value_size = arraysize(value_data);
      DWORD type = 0;
      LRESULT ret = ::RegEnumValue(ua_key.Handle(), value_index, value_name,
                                   &name_size, NULL, &type,
                                   reinterpret_cast<BYTE*>(value_data),
                                   &value_size);
      if (ret == ERROR_SUCCESS) {
        if (StartsWith(value_name, kChromeFramePrefix, false)) {
          ua_key.DeleteValue(value_name);
        } else {
          ++value_index;
        }
      } else {
        break;
      }
    }

    std::wstring chrome_frame_ua_value_name = kChromeFramePrefix;
    chrome_frame_ua_value_name += GetCurrentModuleVersion();
    if (value) {
      ua_key.WriteValue(chrome_frame_ua_value_name.c_str(), value);
    }
    hr = S_OK;
  } else {
    DLOG(ERROR) << __FUNCTION__ << ": " << kPostPlatformUAKey
                << ", error code = " << reg_result;
    hr = HRESULT_FROM_WIN32(reg_result);
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
      return backup_key.WriteValue(NULL, str.GetString()) == ERROR_SUCCESS;
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
    if (backup_key.ReadValue(NULL, NULL, &len, &reg_type) != ERROR_SUCCESS)
      return false;

    if (reg_type != REG_SZ)
      return false;

    size_t wchar_count = 1 + len / sizeof(wchar_t);
    if (backup_key.ReadValue(NULL, WriteInto(sddl, wchar_count), &len,
                             &reg_type) != ERROR_SUCCESS) {
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

HRESULT SetOrDeleteMimeHandlerKey(bool set, HKEY root_key) {
  std::wstring key_name = kInternetSettings;
  key_name.append(L"\\Secure Mime Handlers");
  RegKey key(root_key, key_name.c_str(), KEY_READ | KEY_WRITE);
  if (!key.Valid())
    return false;

  LONG result1 = ERROR_SUCCESS;
  LONG result2 = ERROR_SUCCESS;
  if (set) {
    result1 = key.WriteValue(L"ChromeTab.ChromeActiveDocument", 1);
    result2 = key.WriteValue(L"ChromeTab.ChromeActiveDocument.1", 1);
  } else {
    result1 = key.DeleteValue(L"ChromeTab.ChromeActiveDocument");
    result2 = key.DeleteValue(L"ChromeTab.ChromeActiveDocument.1");
  }

  return result1 != ERROR_SUCCESS ? HRESULT_FROM_WIN32(result1) :
                                    HRESULT_FROM_WIN32(result2);
}

// Chrome Frame registration functions.
//-----------------------------------------------------------------------------
HRESULT RegisterSecuredMimeHandler(bool enable, bool is_system) {
  if (!is_system) {
    return SetOrDeleteMimeHandlerKey(enable, HKEY_CURRENT_USER);
  } else if (base::win::GetVersion() < base::win::VERSION_VISTA) {
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
    return E_ACCESSDENIED;

  // If there is a backup key - something bad happened; try to restore
  // security on "Secure Mime Handlers" from the backup.
  SecurityDescBackup backup(backup_key);
  backup.RestoreSecurity(object_name.c_str());

  // Read old security descriptor of the Mime key first.
  CSecurityDesc sd;
  if (!AtlGetSecurityDescriptor(object_name.c_str(), SE_REGISTRY_KEY, &sd)) {
    return E_FAIL;
  }

  backup.SaveSecurity(sd);
  HRESULT hr = E_FAIL;
  // set new owner
  if (AtlSetOwnerSid(object_name.c_str(), SE_REGISTRY_KEY, token_.GetUser())) {
    // set new dacl
    CDacl new_dacl;
    sd.GetDacl(&new_dacl);
    new_dacl.AddAllowedAce(token_.GetUser(), GENERIC_WRITE | GENERIC_READ);
    if (AtlSetDacl(object_name.c_str(), SE_REGISTRY_KEY, new_dacl)) {
      hr = SetOrDeleteMimeHandlerKey(enable, HKEY_LOCAL_MACHINE);
    }
  }

  backup.RestoreSecurity(object_name.c_str());
  return hr;
}

HRESULT RegisterActiveDoc(bool reg, bool is_system) {
  // We have to call the static T::UpdateRegistry function instead of
  // _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_ACTIVEDOC, reg)
  // because there is specific OLEMISC replacement.
  return ChromeActiveDocument::UpdateRegistry(reg);
}

HRESULT RegisterActiveX(bool reg, bool is_system) {
  // We have to call the static T::UpdateRegistry function instead of
  // _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_ACTIVEX, reg)
  // because there is specific OLEMISC replacement.
  return ChromeFrameActivex::UpdateRegistry(reg);
}

HRESULT RegisterElevationPolicy(bool reg, bool is_system) {
  HRESULT hr = S_OK;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // Register the elevation policy. This must succeed for Chrome Frame to
    // be able launch Chrome when running in low-integrity IE.
    hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_ELEVATION, reg);
    if (SUCCEEDED(hr)) {
      // Ignore failures since old versions of IE 7 (e.g., 7.0.6000.16386, which
      // shipped with Vista RTM) do not export IERefreshElevationPolicy.
      RefreshElevationPolicy();
    }
  }
  return hr;
}

HRESULT RegisterProtocol(bool reg, bool is_system) {
  return _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEPROTOCOL, reg);
}

HRESULT RegisterBhoClsid(bool reg, bool is_system) {
  return Bho::UpdateRegistry(reg);
}

HRESULT RegisterBhoIE(bool reg, bool is_system) {
  if (is_system) {
    return _AtlModule.UpdateRegistryFromResourceS(IDR_REGISTER_BHO, reg);
  } else {
    if (reg) {
      // Setup the long running process:
      return SetupUserLevelHelper();
    } else {
      // Unschedule the user-level helper. Note that we don't kill it here
      // so that during updates we don't have a time window with no running
      // helper. Uninstalls and updates will explicitly kill the helper from
      // within the installer. Unregister existing run-at-startup entry.
      return base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER,
                                                 kRunKeyName) ? S_OK : E_FAIL;
    }
  }
}

HRESULT RegisterTypeLib(bool reg, bool is_system) {
  if (reg && !is_system) {
    // Enables the RegisterTypeLib Function function to override default
    // registry mappings under Windows Vista Service Pack 1 (SP1),
    // Windows Server 2008, and later operating system versions
    typedef void (WINAPI* OaEnablePerUserTypeLibReg)(void);
    OaEnablePerUserTypeLibReg per_user_typelib_func =
        reinterpret_cast<OaEnablePerUserTypeLibReg>(
            GetProcAddress(GetModuleHandle(L"oleaut32.dll"),
                           "OaEnablePerUserTLibRegistration"));
    if (per_user_typelib_func) {
      (*per_user_typelib_func)();
    }
  }
  return reg ?
      UtilRegisterTypeLib(_AtlComModule.m_hInstTypeLib,
                          NULL, !is_system) :
      UtilUnRegisterTypeLib(_AtlComModule.m_hInstTypeLib,
                            NULL, !is_system);
}

HRESULT RegisterLegacyNPAPICleanup(bool reg, bool is_system) {
  if (!reg) {
    _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI, reg);
    UtilRemovePersistentNPAPIMarker();
  }
  // Ignore failures.
  return S_OK;
}

HRESULT RegisterAppId(bool reg, bool is_system) {
  return _AtlModule.UpdateRegistryAppId(reg);
}

HRESULT RegisterUserAgent(bool reg, bool is_system) {
  if (reg) {
    return SetChromeFrameUA(is_system, L"1");
  } else {
    return SetChromeFrameUA(is_system, NULL);
  }
}

enum RegistrationStepId {
  kStepSecuredMimeHandler = 0,
  kStepActiveDoc          = 1,
  kStepActiveX            = 2,
  kStepElevationPolicy    = 3,
  kStepProtocol           = 4,
  kStepBhoClsid           = 5,
  kStepBhoRegistration    = 6,
  kStepRegisterTypeLib    = 7,
  kStepNpapiCleanup       = 8,
  kStepAppId              = 9,
  kStepUserAgent          = 10,
  kStepEnd                = 11
};

enum RegistrationFlags {
  ACTIVEX             = 0x0001,
  ACTIVEDOC           = 0x0002,
  GCF_PROTOCOL        = 0x0004,
  BHO_CLSID           = 0x0008,
  BHO_REGISTRATION    = 0x0010,
  TYPELIB             = 0x0020,

  ALL                 = 0xFFFF
};

// Mux the failure step into the hresult. We take only the first four bits
// and stick those into the top four bits of the facility code. We also set the
// Customer bit to be polite. Graphically, we write our error code to the
// bits marked with ^:
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//       ^   ^ ^ ^ ^
// See http://msdn.microsoft.com/en-us/library/cc231198(PROT.10).aspx for
// more details on HRESULTS.
//
// The resulting error can be extracted by:
// error_code = (fiddled_hr & 0x07800000) >> 23
HRESULT MuxErrorIntoHRESULT(HRESULT hr, int error_code) {
  DCHECK_GE(error_code, 0);
  DCHECK_LT(error_code, kStepEnd);
  COMPILE_ASSERT(kStepEnd <= 0xF, update_error_muxing_too_many_steps);

  // Check that our four desired bits are clear.
  // 0xF87FFFFF == 11111000011111111111111111111111
  DCHECK_EQ(static_cast<HRESULT>(hr & 0xF87FFFFF), hr);

  HRESULT fiddled_hr = ((error_code & 0xF) << 23) | hr;
  fiddled_hr |= 1 << 29;  // Set the customer bit.

  return fiddled_hr;
}

HRESULT CustomRegistration(uint16 reg_flags, bool reg, bool is_system) {
  if (reg && (reg_flags & (ACTIVEDOC | ACTIVEX)))
    reg_flags |= (TYPELIB | GCF_PROTOCOL);

  // Set the flag that gets checked in AddCommonRGSReplacements before doing
  // registration work.
  _AtlModule.do_system_registration_ = is_system;

  typedef HRESULT (*RegistrationFn)(bool reg, bool is_system);
  struct RegistrationStep {
    RegistrationStepId id;
    uint16 condition;
    RegistrationFn func;
  };
  static const RegistrationStep registration_steps[] = {
    { kStepSecuredMimeHandler, ACTIVEDOC, &RegisterSecuredMimeHandler },
    { kStepActiveDoc, ACTIVEDOC, &RegisterActiveDoc },
    { kStepActiveX, ACTIVEX, &RegisterActiveX },
    { kStepElevationPolicy, (ACTIVEDOC | ACTIVEX), &RegisterElevationPolicy },
    { kStepProtocol, GCF_PROTOCOL, &RegisterProtocol },
    { kStepBhoClsid, BHO_CLSID, &RegisterBhoClsid },
    { kStepBhoRegistration, BHO_REGISTRATION, &RegisterBhoIE },
    { kStepRegisterTypeLib, TYPELIB, &RegisterTypeLib },
    { kStepNpapiCleanup, ALL, &RegisterLegacyNPAPICleanup },
    { kStepAppId, ALL, &RegisterAppId },
    { kStepUserAgent, ALL, &RegisterUserAgent }
  };

  HRESULT hr = S_OK;

  bool rollback = false;
  int failure_step = 0;
  HRESULT step_hr = S_OK;
  for (int step = 0; step < arraysize(registration_steps); ++step) {
    if ((reg_flags & registration_steps[step].condition) != 0) {
      step_hr = registration_steps[step].func(reg, is_system);
      if (FAILED(step_hr)) {
        // Store only the first failing HRESULT with the step value muxed in.
        if (hr == S_OK) {
          hr = MuxErrorIntoHRESULT(step_hr, step);
        }

        // On registration if a step fails, we abort and rollback.
        if (reg) {
          rollback = true;
          failure_step = step;
          break;
        }
      }
    }
  }

  if (rollback) {
    DCHECK(reg);
    // Rollback the failing action and all preceding ones.
    for (int step = failure_step; step >= 0; --step) {
      registration_steps[step].func(!reg, is_system);
    }
  }

  return hr;
}

}  // namespace

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
    logging::InitLogging(
        NULL,
        logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
        logging::LOCK_LOG_FILE,
        logging::DELETE_OLD_LOG_FILE,
        logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

    DllRedirector* dll_redirector = DllRedirector::GetInstance();
    DCHECK(dll_redirector);

    if (!dll_redirector->RegisterAsFirstCFModule()) {
      // Someone else was here first, try and get a pointer to their
      // DllGetClassObject export:
      g_dll_get_class_object_redir_ptr =
          dll_redirector->GetDllGetClassObjectPtr();
      DCHECK(g_dll_get_class_object_redir_ptr != NULL)
          << "Found CF module with no DllGetClassObject export.";
    }

    // Enable ETW logging.
    logging::LogEventProvider::Initialize(kChromeFrameProvider);
  } else if (reason == DLL_PROCESS_DETACH) {
    DllRedirector* dll_redirector = DllRedirector::GetInstance();
    DCHECK(dll_redirector);

    dll_redirector->UnregisterAsFirstCFModule();
    g_patch_helper.UnpatchIfNeeded();
    delete g_exit_manager;
    g_exit_manager = NULL;
    ShutdownCrashReporting();
  }
  return _AtlModule.DllMain(reason, reserved);
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

// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer() {
  uint16 flags =  ACTIVEX | ACTIVEDOC | TYPELIB | GCF_PROTOCOL |
                  BHO_CLSID | BHO_REGISTRATION;

  HRESULT hr = CustomRegistration(flags, true, true);
  if (SUCCEEDED(hr)) {
    SetupRunOnce();
  }

  return hr;
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer() {
  HRESULT hr = CustomRegistration(ALL, false, true);
  return hr;
}

// DllRegisterUserServer - Adds entries to the HKCU hive in the registry.
STDAPI DllRegisterUserServer() {
  UINT flags =  ACTIVEX | ACTIVEDOC | TYPELIB | GCF_PROTOCOL |
                BHO_CLSID | BHO_REGISTRATION;

  HRESULT hr = CustomRegistration(flags, TRUE, false);
  if (SUCCEEDED(hr)) {
    SetupRunOnce();
  }

  return hr;
}

// DllUnregisterUserServer - Removes entries from the HKCU hive in the registry.
STDAPI DllUnregisterUserServer() {
  HRESULT hr = CustomRegistration(ALL, FALSE, false);
  return hr;
}

// Object entries go here instead of with each object, so that we can move
// the objects to a lib. Also reduces magic.
OBJECT_ENTRY_AUTO(CLSID_ChromeFrameBHO, Bho)
OBJECT_ENTRY_AUTO(__uuidof(ChromeActiveDocument), ChromeActiveDocument)
OBJECT_ENTRY_AUTO(__uuidof(ChromeFrame), ChromeFrameActivex)
OBJECT_ENTRY_AUTO(__uuidof(ChromeProtocol), ChromeProtocol)
