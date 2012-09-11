// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_policy.h"

#include <string>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "content/common/debug_flags.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/win_utils.h"
#include "ui/gl/gl_switches.h"

static sandbox::BrokerServices* g_broker_services = NULL;
static sandbox::TargetServices* g_target_services = NULL;

namespace {

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the renderer. Note: at runtime we generate short
// versions of the dll name only if the dll has an extension.
const wchar_t* const kTroublesomeDlls[] = {
  L"adialhk.dll",                 // Kaspersky Internet Security.
  L"acpiz.dll",                   // Unknown.
  L"avgrsstx.dll",                // AVG 8.
  L"babylonchromepi.dll",         // Babylon translator.
  L"browsemngr.dll",              // Uninstall blocker used by Babylon.
  L"btkeyind.dll",                // Widcomm Bluetooth.
  L"cmcsyshk.dll",                // CMC Internet Security.
  L"cmsetac.dll",                 // Unknown (suspected malware).
  L"cooliris.dll",                // CoolIris.
  L"dockshellhook.dll",           // Stardock Objectdock.
  L"easyhook32.dll",              // GDIPP and others.
  L"googledesktopnetwork3.dll",   // Google Desktop Search v5.
  L"fwhook.dll",                  // PC Tools Firewall Plus.
  L"hookprocesscreation.dll",     // Blumentals Program protector.
  L"hookterminateapis.dll",       // Blumentals and Cyberprinter.
  L"hookprintapis.dll",           // Cyberprinter.
  L"imon.dll",                    // NOD32 Antivirus.
  L"ioloHL.dll",                  // Iolo (System Mechanic).
  L"kloehk.dll",                  // Kaspersky Internet Security.
  L"lawenforcer.dll",             // Spyware-Browser AntiSpyware (Spybro).
  L"libdivx.dll",                 // DivX.
  L"lvprcinj01.dll",              // Logitech QuickCam.
  L"madchook.dll",                // Madshi (generic hooking library).
  L"mdnsnsp.dll",                 // Bonjour.
  L"moonsysh.dll",                // Moon Secure Antivirus.
  L"mpk.dll",                     // KGB Spy.
  L"npdivx32.dll",                // DivX.
  L"npggNT.des",                  // GameGuard 2008.
  L"npggNT.dll",                  // GameGuard (older).
  L"oawatch.dll",                 // Online Armor.
  L"owexplorer-10513.dll",        // Overwolf.
  L"owexplorer-10514.dll",        // Overwolf.
  L"owexplorer-10515.dll",        // Overwolf.
  L"owexplorer-10516.dll",        // Overwolf.
  L"owexplorer-10517.dll",        // Overwolf.
  L"owexplorer-10518.dll",        // Overwolf.
  L"owexplorer-10519.dll",        // Overwolf.
  L"owexplorer-10520.dll",        // Overwolf.
  L"owexplorer-10521.dll",        // Overwolf.
  L"owexplorer-10522.dll",        // Overwolf.
  L"owexplorer-10523.dll",        // Overwolf.
  L"pavhook.dll",                 // Panda Internet Security.
  L"pavlsphook.dll",              // Panda Antivirus.
  L"pavshook.dll",                // Panda Antivirus.
  L"pavshookwow.dll",             // Panda Antivirus.
  L"pctavhook.dll",               // PC Tools Antivirus.
  L"pctgmhk.dll",                 // PC Tools Spyware Doctor.
  L"prntrack.dll",                // Pharos Systems.
  L"protector.dll",               // Unknown (suspected malware).
  L"radhslib.dll",                // Radiant Naomi Internet Filter.
  L"radprlib.dll",                // Radiant Naomi Internet Filter.
  L"rapportnikko.dll",            // Trustware Rapport.
  L"rlhook.dll",                  // Trustware Bufferzone.
  L"rooksdol.dll",                // Trustware Rapport.
  L"rpchromebrowserrecordhelper.dll",  // RealPlayer.
  L"r3hook.dll",                  // Kaspersky Internet Security.
  L"sahook.dll",                  // McAfee Site Advisor.
  L"sbrige.dll",                  // Unknown.
  L"sc2hook.dll",                 // Supercopier 2.
  L"sdhook32.dll",                // Spybot - Search & Destroy Live Protection.
  L"sguard.dll",                  // Iolo (System Guard).
  L"smum32.dll",                  // Spyware Doctor version 6.
  L"smumhook.dll",                // Spyware Doctor version 5.
  L"ssldivx.dll",                 // DivX.
  L"syncor11.dll",                // SynthCore Midi interface.
  L"systools.dll",                // Panda Antivirus.
  L"tfwah.dll",                   // Threatfire (PC tools).
  L"wblind.dll",                  // Stardock Object desktop.
  L"wbhelp.dll",                  // Stardock Object desktop.
  L"winstylerthemehelper.dll"     // Tuneup utilities 2006.
};

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the plugin process.
const wchar_t* const kTroublesomePluginDlls[] = {
  L"rpmainbrowserrecordplugin.dll",      // RealPlayer.
  L"rpchromebrowserrecordhelper.dll",    // RealPlayer.
  L"rpchrome10browserrecordhelper.dll",  // RealPlayer.
  L"ycwebcamerasource.ax"                // Cyberlink Camera helper.
  L"CLRGL.ax"                            // Cyberlink Camera helper.
};

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the GPU process.
const wchar_t* const kTroublesomeGpuDlls[] = {
  L"cmsetac.dll",                 // Unknown (suspected malware).
};

// Adds the policy rules for the path and path\ with the semantic |access|.
// If |children| is set to true, we need to add the wildcard rules to also
// apply the rule to the subfiles and subfolders.
bool AddDirectory(int path, const wchar_t* sub_dir, bool children,
                  sandbox::TargetPolicy::Semantics access,
                  sandbox::TargetPolicy* policy) {
  FilePath directory;
  if (!PathService::Get(path, &directory))
    return false;

  if (sub_dir) {
    directory = directory.Append(sub_dir);
    file_util::AbsolutePath(&directory);
  }

  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, access,
                           directory.value().c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  std::wstring directory_str = directory.value() + L"\\";
  if (children)
    directory_str += L"*";
  // Otherwise, add the version of the path that ends with a separator.

  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, access,
                           directory_str.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

// Adds the policy rules for the path and path\* with the semantic |access|.
// We need to add the wildcard rules to also apply the rule to the subkeys.
bool AddKeyAndSubkeys(std::wstring key,
                      sandbox::TargetPolicy::Semantics access,
                      sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY, access,
                           key.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  key += L"\\*";
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY, access,
                           key.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

// Compares the loaded |module| file name matches |module_name|.
bool IsExpandedModuleName(HMODULE module, const wchar_t* module_name) {
  wchar_t path[MAX_PATH];
  DWORD sz = ::GetModuleFileNameW(module, path, arraysize(path));
  if ((sz == arraysize(path)) || (sz == 0)) {
    // XP does not set the last error properly, so we bail out anyway.
    return false;
  }
  if (!::GetLongPathName(path, path, arraysize(path)))
    return false;
  FilePath fname(path);
  return (fname.BaseName().value() == module_name);
}

// Adds a single dll by |module_name| into the |policy| blacklist.
// If |check_in_browser| is true we only add an unload policy only if the dll
// is also loaded in this process.
void BlacklistAddOneDll(const wchar_t* module_name,
                        bool check_in_browser,
                        sandbox::TargetPolicy* policy) {
  HMODULE module = check_in_browser ? ::GetModuleHandleW(module_name) : NULL;
  if (!module) {
    // The module could have been loaded with a 8.3 short name. We check
    // the three most common cases: 'thelongname.dll' becomes
    // 'thelon~1.dll', 'thelon~2.dll' and 'thelon~3.dll'.
    std::wstring name(module_name);
    size_t period = name.rfind(L'.');
    DCHECK_NE(std::string::npos, period);
    DCHECK_LE(3U, (name.size() - period));
    if (period <= 8)
      return;
    for (int ix = 0; ix < 3; ++ix) {
      const wchar_t suffix[] = {'~', ('1' + ix), 0};
      std::wstring alt_name = name.substr(0, 6) + suffix;
      alt_name += name.substr(period, name.size());
      if (check_in_browser) {
        module = ::GetModuleHandleW(alt_name.c_str());
        if (!module)
          return;
        // We found it, but because it only has 6 significant letters, we
        // want to make sure it is the right one.
        if (!IsExpandedModuleName(module, module_name))
          return;
      }
      // Found a match. We add both forms to the policy.
      policy->AddDllToUnload(alt_name.c_str());
    }
  }
  policy->AddDllToUnload(module_name);
  DVLOG(1) << "dll to unload found: " << module_name;
  return;
}

// Adds policy rules for unloaded the known dlls that cause chrome to crash.
// Eviction of injected DLLs is done by the sandbox so that the injected module
// does not get a chance to execute any code.
void AddGenericDllEvictionPolicy(sandbox::TargetPolicy* policy) {
  for (int ix = 0; ix != arraysize(kTroublesomeDlls); ++ix)
    BlacklistAddOneDll(kTroublesomeDlls[ix], true, policy);
}

// Same as AddGenericDllEvictionPolicy but specifically for plugins. In this
// case we add the blacklisted dlls even if they are not loaded in this process.
void AddPluginDllEvictionPolicy(sandbox::TargetPolicy* policy) {
  for (int ix = 0; ix != arraysize(kTroublesomePluginDlls); ++ix)
    BlacklistAddOneDll(kTroublesomePluginDlls[ix], false, policy);
}

// Same as AddGenericDllEvictionPolicy but specifically for the GPU process.
// In this we add the blacklisted dlls even if they are not loaded in this
// process.
void AddGpuDllEvictionPolicy(sandbox::TargetPolicy* policy) {
  for (int ix = 0; ix != arraysize(kTroublesomeGpuDlls); ++ix)
    BlacklistAddOneDll(kTroublesomeGpuDlls[ix], false, policy);
}

// Returns the object path prepended with the current logon session.
string16 PrependWindowsSessionPath(const char16* object) {
  // Cache this because it can't change after process creation.
  static uintptr_t s_session_id = 0;
  if (s_session_id == 0) {
    HANDLE token;
    DWORD session_id_length;
    DWORD session_id = 0;

    CHECK(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token));
    CHECK(::GetTokenInformation(token, TokenSessionId, &session_id,
        sizeof(session_id), &session_id_length));
    CloseHandle(token);
    if (session_id)
      s_session_id = session_id;
  }

  return base::StringPrintf(L"\\Sessions\\%d%ls", s_session_id, object);
}

// Closes handles that are opened at process creation and initialization.
void AddBaseHandleClosePolicy(sandbox::TargetPolicy* policy) {
  // Being able to manipulate anything BaseNamedObjects is bad.
  string16 object_path = PrependWindowsSessionPath(L"\\BaseNamedObjects");
  policy->AddKernelObjectToClose(L"Directory", object_path.data());
  object_path = PrependWindowsSessionPath(
      L"\\BaseNamedObjects\\windows_shell_global_counters");
  policy->AddKernelObjectToClose(L"Section", object_path.data());
}

// Adds the generic policy rules to a sandbox TargetPolicy.
bool AddGenericPolicy(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;
  // Allow the server side of a pipe restricted to the "chrome.nacl."
  // namespace so that it cannot impersonate other system or other chrome
  // service pipes.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.nacl.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;
  // Allow the server side of sync sockets, which are pipes that have
  // the "chrome.sync" namespace and a randomly generated suffix.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.sync.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for debug message only in debug
#ifndef NDEBUG
  FilePath app_dir;
  if (!PathService::Get(base::DIR_MODULE, &app_dir))
    return false;

  wchar_t long_path_buf[MAX_PATH];
  DWORD long_path_return_value = GetLongPathName(app_dir.value().c_str(),
                                                 long_path_buf,
                                                 MAX_PATH);
  if (long_path_return_value == 0 || long_path_return_value >= MAX_PATH)
    return false;

  FilePath debug_message(long_path_buf);
  debug_message = debug_message.AppendASCII("debug_message.exe");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                           sandbox::TargetPolicy::PROCESS_MIN_EXEC,
                           debug_message.value().c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif  // NDEBUG

  return true;
}

// For the GPU process we gotten as far as USER_LIMITED. The next level
// which is USER_RESTRICTED breaks both the DirectX backend and the OpenGL
// backend. Note that the GPU process is connected to the interactive
// desktop.
// TODO(cpu): Lock down the sandbox more if possible.
bool AddPolicyForGPU(CommandLine* cmd_line, sandbox::TargetPolicy* policy) {
#if !defined(NACL_WIN64)  // We don't need this code on win nacl64.
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    if (cmd_line->GetSwitchValueASCII(switches::kUseGL) ==
        gfx::kGLImplementationDesktopName) {
      // Open GL path.
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);
      policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
      policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    } else {
      if (cmd_line->GetSwitchValueASCII(switches::kUseGL) ==
          gfx::kGLImplementationSwiftShaderName ||
          cmd_line->HasSwitch(switches::kReduceGpuSandbox) ||
          cmd_line->HasSwitch(switches::kDisableImageTransportSurface)) {
        // Swiftshader path.
        policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                              sandbox::USER_LIMITED);
      } else {
        // Angle + DirectX path.
        policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                              sandbox::USER_RESTRICTED);
        // This is a trick to keep the GPU out of low-integrity processes. It
        // starts at low-integrity for UIPI to work, then drops below
        // low-integrity after warm-up.
        policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
      }

      // UI restrictions break when we access Windows from outside our job.
      // However, we don't want a proxy window in this process because it can
      // introduce deadlocks where the renderer blocks on the gpu, which in
      // turn blocks on the browser UI thread. So, instead we forgo a window
      // message pump entirely and just add job restrictions to prevent child
      // processes.
      policy->SetJobLevel(sandbox::JOB_LIMITED_USER,
                          JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                          JOB_OBJECT_UILIMIT_DESKTOP |
                          JOB_OBJECT_UILIMIT_EXITWINDOWS |
                          JOB_OBJECT_UILIMIT_DISPLAYSETTINGS);

      policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    }
  } else {
    policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
    policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                          sandbox::USER_LIMITED);
  }

  // Allow the server side of GPU sockets, which are pipes that have
  // the "chrome.gpu" namespace and an arbitrary suffix.
  sandbox::ResultCode result = policy->AddRule(
      sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
      sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
      L"\\\\.\\pipe\\chrome.gpu.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // GPU needs to copy sections to renderers.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Section");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

#ifdef USE_AURA
  // GPU also needs to add sections to the browser for aura
  // TODO(jschuh): refactor the GPU channel to remove this. crbug.com/128786
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                           L"Section");
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif

  AddGenericDllEvictionPolicy(policy);
  AddGpuDllEvictionPolicy(policy);
#endif
  return true;
}

bool AddPolicyForRenderer(sandbox::TargetPolicy* policy) {
  // Renderers need to copy sections for plugin DIBs and GPU.
  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Section");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Renderers need to share events with plugins.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Event");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Renderers need to send named pipe handles and shared memory
  // segment handles to NaCl loader processes.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"File");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  policy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);

  sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // On 2003/Vista the initial token has to be restricted if the main
    // token is restricted.
    initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
  }

  policy->SetTokenLevel(initial_token, sandbox::USER_LOCKDOWN);
  // Prevents the renderers from manipulating low-integrity processes.
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);

  bool use_winsta = !CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kDisableAltWinstation);

  if (sandbox::SBOX_ALL_OK !=  policy->SetAlternateDesktop(use_winsta)) {
    DLOG(WARNING) << "Failed to apply desktop security to the renderer";
  }

  AddGenericDllEvictionPolicy(policy);

  return true;
}

// The Pepper process as locked-down as a renderer execpt that it can
// create the server side of chrome pipes.
bool AddPolicyForPepperPlugin(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.*");
  return result == sandbox::SBOX_ALL_OK;
}

// This code is test only, and attempts to catch unsafe uses of
// DuplicateHandle() that copy privileged handles into sandboxed processes.
#ifndef OFFICIAL_BUILD
base::win::IATPatchFunction g_iat_patch_duplicate_handle;

BOOL (WINAPI *g_iat_orig_duplicate_handle)(HANDLE source_process_handle,
                                           HANDLE source_handle,
                                           HANDLE target_process_handle,
                                           LPHANDLE target_handle,
                                           DWORD desired_access,
                                           BOOL inherit_handle,
                                           DWORD options);

NtQueryObject g_QueryObject = NULL;

static const char* kDuplicateHandleWarning =
    "You are attempting to duplicate a privileged handle into a sandboxed"
    " process.\n Please use the sandbox::BrokerDuplicateHandle API or"
    " contact security@chromium.org for assistance.";

void CheckDuplicateHandle(HANDLE handle) {
  // Get the object type (32 characters is safe; current max is 14).
  BYTE buffer[sizeof(OBJECT_TYPE_INFORMATION) + 32 * sizeof(wchar_t)];
  OBJECT_TYPE_INFORMATION* type_info =
      reinterpret_cast<OBJECT_TYPE_INFORMATION*>(buffer);
  ULONG size = sizeof(buffer) - sizeof(wchar_t);
  NTSTATUS error;
  error = g_QueryObject(handle, ObjectTypeInformation, type_info, size, &size);
  CHECK(NT_SUCCESS(error));
  type_info->Name.Buffer[type_info->Name.Length / sizeof(wchar_t)] = L'\0';

  // Get the object basic information.
  OBJECT_BASIC_INFORMATION basic_info;
  size = sizeof(basic_info);
  error = g_QueryObject(handle, ObjectBasicInformation, &basic_info, size,
                        &size);
  CHECK(NT_SUCCESS(error));

  if (0 == _wcsicmp(type_info->Name.Buffer, L"Process")) {
    const ACCESS_MASK kDangerousMask = ~(PROCESS_QUERY_LIMITED_INFORMATION |
                                         SYNCHRONIZE);
    CHECK(!(basic_info.GrantedAccess & kDangerousMask)) <<
        kDuplicateHandleWarning;
  }
}

BOOL WINAPI DuplicateHandlePatch(HANDLE source_process_handle,
                                 HANDLE source_handle,
                                 HANDLE target_process_handle,
                                 LPHANDLE target_handle,
                                 DWORD desired_access,
                                 BOOL inherit_handle,
                                 DWORD options) {
  // Duplicate the handle so we get the final access mask.
  if (!g_iat_orig_duplicate_handle(source_process_handle, source_handle,
                                   target_process_handle, target_handle,
                                   desired_access, inherit_handle, options))
    return FALSE;

  // We're not worried about broker handles or not crossing process boundaries.
  if (source_process_handle == target_process_handle ||
      target_process_handle == ::GetCurrentProcess())
    return TRUE;

  // Only sandboxed children are placed in jobs, so just check them.
  BOOL is_in_job = FALSE;
  if (!::IsProcessInJob(target_process_handle, NULL, &is_in_job)) {
    // We need a handle with permission to check the job object.
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      base::win::ScopedHandle process;
      CHECK(g_iat_orig_duplicate_handle(::GetCurrentProcess(),
                                        target_process_handle,
                                        ::GetCurrentProcess(),
                                        process.Receive(),
                                        PROCESS_QUERY_INFORMATION,
                                        FALSE, 0));
      CHECK(::IsProcessInJob(process, NULL, &is_in_job));
    }
  }

  if (is_in_job) {
    // We never allow inheritable child handles.
    CHECK(!inherit_handle) << kDuplicateHandleWarning;

    // Duplicate the handle again, to get the final permissions.
    base::win::ScopedHandle handle;
    CHECK(g_iat_orig_duplicate_handle(target_process_handle, *target_handle,
                                      ::GetCurrentProcess(), handle.Receive(),
                                      0, FALSE, DUPLICATE_SAME_ACCESS));

    // Callers use CHECK macro to make sure we get the right stack.
    CheckDuplicateHandle(handle);
  }

  return TRUE;
}
#endif

}  // namespace

namespace sandbox {

bool InitBrokerServices(sandbox::BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  DCHECK(broker_services);
  DCHECK(!g_broker_services);
  sandbox::ResultCode result = broker_services->Init();
  g_broker_services = broker_services;

// In non-official builds warn about dangerous uses of DuplicateHandle.
  BOOL is_in_job = FALSE;
#ifdef NACL_WIN64
  CHECK(::IsProcessInJob(::GetCurrentProcess(), NULL, &is_in_job));
#endif
#ifndef OFFICIAL_BUILD
  if (!is_in_job && !g_iat_patch_duplicate_handle.is_patched()) {
    HMODULE module = NULL;
    wchar_t module_name[MAX_PATH];
    CHECK(::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              reinterpret_cast<LPCWSTR>(InitBrokerServices),
                              &module));
    DWORD result = ::GetModuleFileNameW(module, module_name, MAX_PATH);
    if (result && (result != MAX_PATH)) {
      ResolveNTFunctionPtr("NtQueryObject", &g_QueryObject);
      g_iat_orig_duplicate_handle = ::DuplicateHandle;
      g_iat_patch_duplicate_handle.Patch(
          module_name, "kernel32.dll", "DuplicateHandle",
          DuplicateHandlePatch);
    }
  }
#endif

  return SBOX_ALL_OK == result;
}

bool InitTargetServices(sandbox::TargetServices* target_services) {
  DCHECK(target_services);
  DCHECK(!g_target_services);
  sandbox::ResultCode result = target_services->Init();
  g_target_services = target_services;
  return SBOX_ALL_OK == result;
}

base::ProcessHandle StartProcessWithAccess(CommandLine* cmd_line,
                                           const FilePath& exposed_dir) {
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  content::ProcessType type;
  std::string type_str = cmd_line->GetSwitchValueASCII(switches::kProcessType);
  if (type_str == switches::kRendererProcess) {
    type = content::PROCESS_TYPE_RENDERER;
  } else if (type_str == switches::kPluginProcess) {
    type = content::PROCESS_TYPE_PLUGIN;
  } else if (type_str == switches::kWorkerProcess) {
    type = content::PROCESS_TYPE_WORKER;
  } else if (type_str == switches::kNaClLoaderProcess) {
    type = content::PROCESS_TYPE_NACL_LOADER;
  } else if (type_str == switches::kUtilityProcess) {
    type = content::PROCESS_TYPE_UTILITY;
  } else if (type_str == switches::kNaClBrokerProcess) {
    type = content::PROCESS_TYPE_NACL_BROKER;
  } else if (type_str == switches::kGpuProcess) {
    type = content::PROCESS_TYPE_GPU;
  } else if (type_str == switches::kPpapiPluginProcess) {
    type = content::PROCESS_TYPE_PPAPI_PLUGIN;
  } else if (type_str == switches::kPpapiBrokerProcess) {
    type = content::PROCESS_TYPE_PPAPI_BROKER;
  } else {
    NOTREACHED();
    return 0;
  }

  TRACE_EVENT_BEGIN_ETW("StartProcessWithAccess", 0, type_str);

  // To decide if the process is going to be sandboxed we have two cases.
  // First case: all process types except the nacl broker, and the plugin
  // process are sandboxed by default.
  bool in_sandbox =
      (type != content::PROCESS_TYPE_NACL_BROKER) &&
      (type != content::PROCESS_TYPE_PLUGIN) &&
      (type != content::PROCESS_TYPE_PPAPI_BROKER);

  // If it is the GPU process then it can be disabled by a command line flag.
  if ((type == content::PROCESS_TYPE_GPU) &&
      (cmd_line->HasSwitch(switches::kDisableGpuSandbox))) {
    in_sandbox = false;
    DVLOG(1) << "GPU sandbox is disabled";
  }

  if (browser_command_line.HasSwitch(switches::kNoSandbox) ||
      cmd_line->HasSwitch(switches::kNoSandbox)) {
    // The user or the caller has explicity opted-out from all sandboxing.
    in_sandbox = false;
  }

#if !defined (GOOGLE_CHROME_BUILD)
  if (browser_command_line.HasSwitch(switches::kInProcessPlugins)) {
    // In process plugins won't work if the sandbox is enabled.
    in_sandbox = false;
  }
#endif
  if (!browser_command_line.HasSwitch(switches::kDisable3DAPIs) &&
      !browser_command_line.HasSwitch(switches::kDisableExperimentalWebGL) &&
      browser_command_line.HasSwitch(switches::kInProcessWebGL)) {
    // In process WebGL won't work if the sandbox is enabled.
    in_sandbox = false;
  }

  // Propagate the Chrome Frame flag to sandboxed processes if present.
  if (browser_command_line.HasSwitch(switches::kChromeFrame)) {
    if (!cmd_line->HasSwitch(switches::kChromeFrame)) {
      cmd_line->AppendSwitch(switches::kChromeFrame);
    }
  }

  bool child_needs_help =
      DebugFlags::ProcessDebugFlags(cmd_line, type, in_sandbox);

  // Prefetch hints on windows:
  // Using a different prefetch profile per process type will allow Windows
  // to create separate pretetch settings for browser, renderer etc.
  cmd_line->AppendArg(base::StringPrintf("/prefetch:%d", type));

  sandbox::ResultCode result;
  base::win::ScopedProcessInformation target;
  sandbox::TargetPolicy* policy = g_broker_services->CreatePolicy();

#if !defined(NACL_WIN64)  // We don't need this code on win nacl64.
  if (type == content::PROCESS_TYPE_PLUGIN &&
      !browser_command_line.HasSwitch(switches::kNoSandbox) &&
      content::GetContentClient()->SandboxPlugin(cmd_line, policy)) {
    in_sandbox = true;
  }
#endif

  if (!in_sandbox) {
    policy->Release();
    base::ProcessHandle process = 0;
    base::LaunchProcess(*cmd_line, base::LaunchOptions(), &process);
    g_broker_services->AddTargetPeer(process);
    return process;
  }

  if (type == content::PROCESS_TYPE_PLUGIN) {
    AddGenericDllEvictionPolicy(policy);
    AddPluginDllEvictionPolicy(policy);
  } else if (type == content::PROCESS_TYPE_GPU) {
    if (!AddPolicyForGPU(cmd_line, policy))
      return 0;
  } else {
    if (!AddPolicyForRenderer(policy))
      return 0;
    // TODO(jschuh): Need get these restrictions applied to NaCl and Pepper.
    // Just have to figure out what needs to be warmed up first.
    if (type == content::PROCESS_TYPE_RENDERER ||
        type == content::PROCESS_TYPE_WORKER) {
      AddBaseHandleClosePolicy(policy);
    }

    // Pepper uses the renderer's policy, whith some tweaks.
    if (cmd_line->HasSwitch(switches::kGuestRenderer) ||
        type == content::PROCESS_TYPE_PPAPI_PLUGIN) {
      if (!AddPolicyForPepperPlugin(policy))
        return 0;
    }


    if (type_str != switches::kRendererProcess) {
      // Hack for Google Desktop crash. Trick GD into not injecting its DLL into
      // this subprocess. See
      // http://code.google.com/p/chromium/issues/detail?id=25580
      cmd_line->AppendSwitchASCII("ignored", " --type=renderer ");
    }
  }

  if (!exposed_dir.empty()) {
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_dir.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return 0;

    FilePath exposed_files = exposed_dir.AppendASCII("*");
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             exposed_files.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return 0;
  }

  if (!AddGenericPolicy(policy)) {
    NOTREACHED();
    return 0;
  }

  TRACE_EVENT_BEGIN_ETW("StartProcessWithAccess::LAUNCHPROCESS", 0, 0);

  result = g_broker_services->SpawnTarget(
      cmd_line->GetProgram().value().c_str(),
      cmd_line->GetCommandLineString().c_str(),
      policy, target.Receive());
  policy->Release();

  TRACE_EVENT_END_ETW("StartProcessWithAccess::LAUNCHPROCESS", 0, 0);

  if (sandbox::SBOX_ALL_OK != result) {
    DLOG(ERROR) << "Failed to launch process. Error: " << result;
    return 0;
  }

#if !defined(NACL_WIN64)
  // For Native Client sel_ldr processes on 32-bit Windows, reserve 1 GB of
  // address space to prevent later failure due to address space fragmentation
  // from .dll loading. The NaCl process will attempt to locate this space by
  // scanning the address space using VirtualQuery.
  // TODO(bbudge) Handle the --no-sandbox case.
  // http://code.google.com/p/nativeclient/issues/detail?id=2131
  if (type == content::PROCESS_TYPE_NACL_LOADER) {
    const SIZE_T kOneGigabyte = 1 << 30;
    void* nacl_mem = VirtualAllocEx(target.process_handle(),
                                    NULL,
                                    kOneGigabyte,
                                    MEM_RESERVE,
                                    PAGE_NOACCESS);
    if (!nacl_mem) {
      DLOG(WARNING) << "Failed to reserve address space for Native Client";
    }
  }
#endif  // !defined(NACL_WIN64)

  ResumeThread(target.thread_handle());

  // Help the process a little. It can't start the debugger by itself if
  // the process is in a sandbox.
  if (child_needs_help)
    base::debug::SpawnDebuggerOnProcess(target.process_id());

  return target.TakeProcessHandle();
}

}  // namespace sandbox

namespace content {

bool BrokerDuplicateHandle(HANDLE source_handle,
                           DWORD target_process_id,
                           HANDLE* target_handle,
                           DWORD desired_access,
                           DWORD options) {
  // If our process is the target just duplicate the handle.
  if (::GetCurrentProcessId() == target_process_id) {
    return !!::DuplicateHandle(::GetCurrentProcess(), source_handle,
                               ::GetCurrentProcess(), target_handle,
                               desired_access, FALSE, options);

  }

  // Try the broker next
  if (g_target_services &&
      g_target_services->DuplicateHandle(source_handle, target_process_id,
                                         target_handle, desired_access,
                                         options) == sandbox::SBOX_ALL_OK) {
    return true;
  }

  // Finally, see if we already have access to the process.
  base::win::ScopedHandle target_process;
  target_process.Set(::OpenProcess(PROCESS_DUP_HANDLE, FALSE,
                                    target_process_id));
  if (target_process.IsValid()) {
    return !!::DuplicateHandle(::GetCurrentProcess(), source_handle,
                                target_process, target_handle,
                                desired_access, FALSE, options);
  }

  return false;
}

bool BrokerAddTargetPeer(HANDLE peer_process) {
  return g_broker_services->AddTargetPeer(peer_process) == sandbox::SBOX_ALL_OK;
}

base::ProcessHandle StartProcessWithAccess(
    CommandLine* cmd_line,
    const FilePath& exposed_dir) {
  return sandbox::StartProcessWithAccess(cmd_line, exposed_dir);
}

}  // namespace content
