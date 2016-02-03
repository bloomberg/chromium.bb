// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_win.h"

#include <stddef.h>

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/dwrite_font_platform_win.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/win_utils.h"

#if !defined(NACL_WIN64)
#include "ui/gfx/win/direct_write.h" // nogncheck: unused #ifdef NACL_WIN64
#endif  // !defined(NACL_WIN64)

static sandbox::BrokerServices* g_broker_services = NULL;
static sandbox::TargetServices* g_target_services = NULL;

namespace content {
namespace {

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the renderer. Note: at runtime we generate short
// versions of the dll name only if the dll has an extension.
// For more information about how this list is generated, and how to get off
// of it, see:
// https://sites.google.com/a/chromium.org/dev/Home/third-party-developers
const wchar_t* const kTroublesomeDlls[] = {
  L"adialhk.dll",                 // Kaspersky Internet Security.
  L"acpiz.dll",                   // Unknown.
  L"activedetect32.dll",          // Lenovo One Key Theater (crbug.com/536056).
  L"activedetect64.dll",          // Lenovo One Key Theater (crbug.com/536056).
  L"airfoilinject3.dll",          // Airfoil.
  L"akinsofthook32.dll",          // Akinsoft Software Engineering.
  L"assistant_x64.dll",           // Unknown.
  L"avcuf64.dll",                 // Bit Defender Internet Security x64.
  L"avgrsstx.dll",                // AVG 8.
  L"babylonchromepi.dll",         // Babylon translator.
  L"btkeyind.dll",                // Widcomm Bluetooth.
  L"cmcsyshk.dll",                // CMC Internet Security.
  L"cmsetac.dll",                 // Unknown (suspected malware).
  L"cooliris.dll",                // CoolIris.
  L"cplushook.dll",               // Unknown (suspected malware).
  L"dockshellhook.dll",           // Stardock Objectdock.
  L"easyhook32.dll",              // GDIPP and others.
  L"esspd.dll",                   // Samsung Smart Security ESCORT.
  L"googledesktopnetwork3.dll",   // Google Desktop Search v5.
  L"fwhook.dll",                  // PC Tools Firewall Plus.
  L"hookprocesscreation.dll",     // Blumentals Program protector.
  L"hookterminateapis.dll",       // Blumentals and Cyberprinter.
  L"hookprintapis.dll",           // Cyberprinter.
  L"imon.dll",                    // NOD32 Antivirus.
  L"icatcdll.dll",                // Samsung Smart Security ESCORT.
  L"icdcnl.dll",                  // Samsung Smart Security ESCORT.
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
  L"pastali32.dll",               // PastaLeads.
  L"pavhook.dll",                 // Panda Internet Security.
  L"pavlsphook.dll",              // Panda Antivirus.
  L"pavshook.dll",                // Panda Antivirus.
  L"pavshookwow.dll",             // Panda Antivirus.
  L"pctavhook.dll",               // PC Tools Antivirus.
  L"pctgmhk.dll",                 // PC Tools Spyware Doctor.
  L"picrmi32.dll",                // PicRec.
  L"picrmi64.dll",                // PicRec.
  L"prntrack.dll",                // Pharos Systems.
  L"protector.dll",               // Unknown (suspected malware).
  L"radhslib.dll",                // Radiant Naomi Internet Filter.
  L"radprlib.dll",                // Radiant Naomi Internet Filter.
  L"rapportnikko.dll",            // Trustware Rapport.
  L"rlhook.dll",                  // Trustware Bufferzone.
  L"rooksdol.dll",                // Trustware Rapport.
  L"rndlpepperbrowserrecordhelper.dll", // RealPlayer.
  L"rpchromebrowserrecordhelper.dll",   // RealPlayer.
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
  L"windowsapihookdll32.dll",     // Lenovo One Key Theater (crbug.com/536056).
  L"windowsapihookdll64.dll",     // Lenovo One Key Theater (crbug.com/536056).
  L"winstylerthemehelper.dll"     // Tuneup utilities 2006.
};

#if !defined(NACL_WIN64)
// Adds the policy rules for the path and path\ with the semantic |access|.
// If |children| is set to true, we need to add the wildcard rules to also
// apply the rule to the subfiles and subfolders.
bool AddDirectory(int path, const wchar_t* sub_dir, bool children,
                  sandbox::TargetPolicy::Semantics access,
                  sandbox::TargetPolicy* policy) {
  base::FilePath directory;
  if (!PathService::Get(path, &directory))
    return false;

  if (sub_dir)
    directory = base::MakeAbsoluteFilePath(directory.Append(sub_dir));

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
#endif  // !defined(NACL_WIN64)

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
  base::FilePath fname(path);
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
    for (wchar_t ix = '1'; ix <= '3'; ++ix) {
      const wchar_t suffix[] = {'~', ix, 0};
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

// Returns the object path prepended with the current logon session.
base::string16 PrependWindowsSessionPath(const base::char16* object) {
  // Cache this because it can't change after process creation.
  static DWORD s_session_id = 0;
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

  return base::StringPrintf(L"\\Sessions\\%lu%ls", s_session_id, object);
}

// Checks if the sandbox should be let to run without a job object assigned.
bool ShouldSetJobLevel(const base::CommandLine& cmd_line) {
  if (!cmd_line.HasSwitch(switches::kAllowNoSandboxJob))
    return true;

  // Windows 8 allows nested jobs so we don't need to check if we are in other
  // job.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return true;

  BOOL in_job = true;
  // Either there is no job yet associated so we must add our job,
  if (!::IsProcessInJob(::GetCurrentProcess(), NULL, &in_job))
    NOTREACHED() << "IsProcessInJob failed. " << GetLastError();
  if (!in_job)
    return true;

  // ...or there is a job but the JOB_OBJECT_LIMIT_BREAKAWAY_OK limit is set.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {};
  if (!::QueryInformationJobObject(NULL,
                                   JobObjectExtendedLimitInformation, &job_info,
                                   sizeof(job_info), NULL)) {
    NOTREACHED() << "QueryInformationJobObject failed. " << GetLastError();
    return true;
  }
  if (job_info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK)
    return true;

  return false;
}

// Adds the generic policy rules to a sandbox TargetPolicy.
bool AddGenericPolicy(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;

  // Renderers need to copy sections for plugin DIBs and GPU.
  // GPU needs to copy sections to renderers.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Section");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for the client side of a pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome." so the sandboxed process cannot connect to system services.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for the server side of nacl pipe. It is just a file
  // in the \pipe\ namespace. We restrict it to pipes that start with
  // "chrome.nacl" so the sandboxed process cannot connect to
  // system services.
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
  base::FilePath app_dir;
  if (!PathService::Get(base::DIR_MODULE, &app_dir))
    return false;

  wchar_t long_path_buf[MAX_PATH];
  DWORD long_path_return_value = GetLongPathName(app_dir.value().c_str(),
                                                 long_path_buf,
                                                 MAX_PATH);
  if (long_path_return_value == 0 || long_path_return_value >= MAX_PATH)
    return false;

  base::FilePath debug_message(long_path_buf);
  debug_message = debug_message.AppendASCII("debug_message.exe");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                           sandbox::TargetPolicy::PROCESS_MIN_EXEC,
                           debug_message.value().c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif  // NDEBUG

  // Add the policy for read-only PDB file access for stack traces.
#if !defined(OFFICIAL_BUILD)
  base::FilePath exe;
  if (!PathService::Get(base::FILE_EXE, &exe))
    return false;
  base::FilePath pdb_path = exe.DirName().Append(L"*.pdb");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                           pdb_path.value().c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif

#if defined(SANITIZER_COVERAGE)
  DWORD coverage_dir_size =
      ::GetEnvironmentVariable(L"SANITIZER_COVERAGE_DIR", NULL, 0);
  if (coverage_dir_size == 0) {
    LOG(WARNING) << "SANITIZER_COVERAGE_DIR was not set, coverage won't work.";
  } else {
    std::wstring coverage_dir;
    wchar_t* coverage_dir_str =
        base::WriteInto(&coverage_dir, coverage_dir_size);
    coverage_dir_size = ::GetEnvironmentVariable(
        L"SANITIZER_COVERAGE_DIR", coverage_dir_str, coverage_dir_size);
    CHECK(coverage_dir.size() == coverage_dir_size);
    base::FilePath sancov_path =
        base::FilePath(coverage_dir).Append(L"*.sancov");
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                             sandbox::TargetPolicy::FILES_ALLOW_ANY,
                             sancov_path.value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return false;
  }
#endif

  AddGenericDllEvictionPolicy(policy);
  return true;
}

bool AddPolicyForSandboxedProcess(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;
  // Renderers need to share events with plugins.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"Event");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Win8+ adds a device DeviceApi that we don't need.
  if (base::win::GetVersion() > base::win::VERSION_WIN7)
    result = policy->AddKernelObjectToClose(L"File", L"\\Device\\DeviceApi");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Close the proxy settings on XP.
  if (base::win::GetVersion() <= base::win::VERSION_SERVER_2003)
    result = policy->AddKernelObjectToClose(L"Key",
                 L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\" \
                     L"CurrentVersion\\Internet Settings");
  if (result != sandbox::SBOX_ALL_OK)
    return false;


  sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // On 2003/Vista the initial token has to be restricted if the main
    // token is restricted.
    initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
  }

  policy->SetTokenLevel(initial_token, sandbox::USER_LOCKDOWN);
  // Prevents the renderers from manipulating low-integrity processes.
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  if (sandbox::SBOX_ALL_OK !=  policy->SetAlternateDesktop(true)) {
    DLOG(WARNING) << "Failed to apply desktop security to the renderer";
  }

  return true;
}

// Updates the command line arguments with debug-related flags. If debug flags
// have been used with this process, they will be filtered and added to
// command_line as needed.
void ProcessDebugFlags(base::CommandLine* command_line) {
  const base::CommandLine& current_cmd_line =
      *base::CommandLine::ForCurrentProcess();
  std::string type = command_line->GetSwitchValueASCII(switches::kProcessType);
  if (current_cmd_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value = current_cmd_line.GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    if (value.empty() || value == type) {
      command_line->AppendSwitch(switches::kWaitForDebugger);
    }
    command_line->AppendSwitchASCII(switches::kWaitForDebuggerChildren, value);
  }
}

// This code is test only, and attempts to catch unsafe uses of
// DuplicateHandle() that copy privileged handles into sandboxed processes.
#ifndef OFFICIAL_BUILD
base::win::IATPatchFunction g_iat_patch_duplicate_handle;

typedef BOOL (WINAPI *DuplicateHandleFunctionPtr)(HANDLE source_process_handle,
                                                  HANDLE source_handle,
                                                  HANDLE target_process_handle,
                                                  LPHANDLE target_handle,
                                                  DWORD desired_access,
                                                  BOOL inherit_handle,
                                                  DWORD options);

DuplicateHandleFunctionPtr g_iat_orig_duplicate_handle;

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

  CHECK(!(basic_info.GrantedAccess & WRITE_DAC)) <<
      kDuplicateHandleWarning;

  if (0 == _wcsicmp(type_info->Name.Buffer, L"Process")) {
    const ACCESS_MASK kDangerousMask =
        ~static_cast<DWORD>(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
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
      HANDLE temp_handle;
      CHECK(g_iat_orig_duplicate_handle(::GetCurrentProcess(),
                                        target_process_handle,
                                        ::GetCurrentProcess(),
                                        &temp_handle,
                                        PROCESS_QUERY_INFORMATION,
                                        FALSE, 0));
      base::win::ScopedHandle process(temp_handle);
      CHECK(::IsProcessInJob(process.Get(), NULL, &is_in_job));
    }
  }

  if (is_in_job) {
    // We never allow inheritable child handles.
    CHECK(!inherit_handle) << kDuplicateHandleWarning;

    // Duplicate the handle again, to get the final permissions.
    HANDLE temp_handle;
    CHECK(g_iat_orig_duplicate_handle(target_process_handle, *target_handle,
                                      ::GetCurrentProcess(), &temp_handle,
                                      0, FALSE, DUPLICATE_SAME_ACCESS));
    base::win::ScopedHandle handle(temp_handle);

    // Callers use CHECK macro to make sure we get the right stack.
    CheckDuplicateHandle(handle.Get());
  }

  return TRUE;
}
#endif

bool IsAppContainerEnabled() {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string appcontainer_group_name =
      base::FieldTrialList::FindFullName("EnableAppContainer");
  if (command_line.HasSwitch(switches::kDisableAppContainer))
    return false;
  if (command_line.HasSwitch(switches::kEnableAppContainer))
    return true;
  return base::StartsWith(appcontainer_group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

void SetJobLevel(const base::CommandLine& cmd_line,
                 sandbox::JobLevel job_level,
                 uint32_t ui_exceptions,
                 sandbox::TargetPolicy* policy) {
  if (ShouldSetJobLevel(cmd_line)) {
#ifdef _WIN64
    policy->SetJobMemoryLimit(4ULL * 1024 * 1024 * 1024);
#endif
    policy->SetJobLevel(job_level, ui_exceptions);
  } else {
    policy->SetJobLevel(sandbox::JOB_NONE, 0);
  }
}

// TODO(jschuh): Need get these restrictions applied to NaCl and Pepper.
// Just have to figure out what needs to be warmed up first.
void AddBaseHandleClosePolicy(sandbox::TargetPolicy* policy) {
  // TODO(cpu): Add back the BaseNamedObjects policy.
  base::string16 object_path = PrependWindowsSessionPath(
      L"\\BaseNamedObjects\\windows_shell_global_counters");
  policy->AddKernelObjectToClose(L"Section", object_path.data());
}

void AddAppContainerPolicy(sandbox::TargetPolicy* policy, const wchar_t* sid) {
  if (IsAppContainerEnabled())
    policy->SetLowBox(sid);
}

bool AddWin32kLockdownPolicy(sandbox::TargetPolicy* policy) {
#if !defined(NACL_WIN64)
  if (!IsWin32kRendererLockdownEnabled())
    return true;

  // Enable win32k lockdown if not already.
  sandbox::MitigationFlags flags = policy->GetProcessMitigations();
  if ((flags & sandbox::MITIGATION_WIN32K_DISABLE) ==
      sandbox::MITIGATION_WIN32K_DISABLE)
    return true;

  sandbox::ResultCode result =
      policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                      sandbox::TargetPolicy::FAKE_USER_GDI_INIT, nullptr);
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  flags |= sandbox::MITIGATION_WIN32K_DISABLE;
  result = policy->SetProcessMitigations(flags);
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif
  return true;
}

bool InitBrokerServices(sandbox::BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  DCHECK(broker_services);
  DCHECK(!g_broker_services);
  sandbox::ResultCode result = broker_services->Init();
  g_broker_services = broker_services;

  // In non-official builds warn about dangerous uses of DuplicateHandle.
#ifndef OFFICIAL_BUILD
  BOOL is_in_job = FALSE;
  CHECK(::IsProcessInJob(::GetCurrentProcess(), NULL, &is_in_job));
  // In a Syzygy-profiled binary, instrumented for import profiling, this
  // patch will end in infinite recursion on the attempted delegation to the
  // original function.
  if (!base::debug::IsBinaryInstrumented() &&
      !is_in_job && !g_iat_patch_duplicate_handle.is_patched()) {
    HMODULE module = NULL;
    wchar_t module_name[MAX_PATH];
    CHECK(::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              reinterpret_cast<LPCWSTR>(InitBrokerServices),
                              &module));
    DWORD result = ::GetModuleFileNameW(module, module_name, MAX_PATH);
    if (result && (result != MAX_PATH)) {
      ResolveNTFunctionPtr("NtQueryObject", &g_QueryObject);
      result = g_iat_patch_duplicate_handle.Patch(
          module_name, "kernel32.dll", "DuplicateHandle",
          DuplicateHandlePatch);
      CHECK(result == 0);
      g_iat_orig_duplicate_handle =
          reinterpret_cast<DuplicateHandleFunctionPtr>(
              g_iat_patch_duplicate_handle.original_function());
    }
  }
#endif

  return sandbox::SBOX_ALL_OK == result;
}

bool InitTargetServices(sandbox::TargetServices* target_services) {
  DCHECK(target_services);
  DCHECK(!g_target_services);
  sandbox::ResultCode result = target_services->Init();
  g_target_services = target_services;
  return sandbox::SBOX_ALL_OK == result;
}

base::Process StartSandboxedProcess(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line) {
  DCHECK(delegate);
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string type_str = cmd_line->GetSwitchValueASCII(switches::kProcessType);

  TRACE_EVENT1("startup", "StartProcessWithAccess", "type", type_str);

  // Propagate the --allow-no-job flag if present.
  if (browser_command_line.HasSwitch(switches::kAllowNoSandboxJob) &&
      !cmd_line->HasSwitch(switches::kAllowNoSandboxJob)) {
    cmd_line->AppendSwitch(switches::kAllowNoSandboxJob);
  }

  ProcessDebugFlags(cmd_line);

  if ((!delegate->ShouldSandbox()) ||
      browser_command_line.HasSwitch(switches::kNoSandbox) ||
      cmd_line->HasSwitch(switches::kNoSandbox)) {
    base::Process process =
        base::LaunchProcess(*cmd_line, base::LaunchOptions());
    // TODO(rvargas) crbug.com/417532: Don't share a raw handle.
    g_broker_services->AddTargetPeer(process.Handle());
    return process.Pass();
  }

  sandbox::TargetPolicy* policy = g_broker_services->CreatePolicy();

  // Pre-startup mitigations.
  sandbox::MitigationFlags mitigations =
      sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_BOTTOM_UP_ASLR |
      sandbox::MITIGATION_DEP |
      sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_SEHOP;

  if (policy->SetProcessMitigations(mitigations) != sandbox::SBOX_ALL_OK)
    return base::Process();

#if !defined(NACL_WIN64)
  if (type_str == switches::kRendererProcess &&
      IsWin32kRendererLockdownEnabled()) {
    if (!AddWin32kLockdownPolicy(policy))
      return base::Process();
  }
#endif

  // Post-startup mitigations.
  mitigations = sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
                sandbox::MITIGATION_DLL_SEARCH_ORDER;

  if (policy->SetDelayedProcessMitigations(mitigations) != sandbox::SBOX_ALL_OK)
    return base::Process();

  SetJobLevel(*cmd_line, sandbox::JOB_LOCKDOWN, 0, policy);

  if (!delegate->DisableDefaultPolicy()) {
    if (!AddPolicyForSandboxedProcess(policy))
      return base::Process();
  }

#if !defined(NACL_WIN64)
  if (type_str == switches::kRendererProcess ||
      type_str == switches::kPpapiPluginProcess) {
    if (gfx::win::ShouldUseDirectWrite()) {
      AddDirectory(base::DIR_WINDOWS_FONTS,
                  NULL,
                  true,
                  sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                  policy);

      if (!ShouldUseDirectWriteFontProxyFieldTrial()) {
        // If DirectWrite is enabled for font rendering then open the font
        // cache section which is created by the browser and pass the handle to
        // the renderer process. This is needed because renderer processes on
        // Windows 8+ may be running in an AppContainer sandbox and hence their
        // kernel object namespace may be partitioned.
        std::string name(content::kFontCacheSharedSectionName);
        name.append(base::UintToString(base::GetCurrentProcId()));

        base::SharedMemory direct_write_font_cache_section;
        if (direct_write_font_cache_section.Open(name, true)) {
          void* shared_handle = policy->AddHandleToShare(
              direct_write_font_cache_section.handle().GetHandle());
          cmd_line->AppendSwitchASCII(
              switches::kFontCacheSharedHandle,
              base::UintToString(base::win::HandleToUint32(shared_handle)));
        }
      }
    }
  }
#endif

  if (type_str != switches::kRendererProcess) {
    // Hack for Google Desktop crash. Trick GD into not injecting its DLL into
    // this subprocess. See
    // http://code.google.com/p/chromium/issues/detail?id=25580
    cmd_line->AppendSwitchASCII("ignored", " --type=renderer ");
  }

  if (!AddGenericPolicy(policy)) {
    NOTREACHED();
    return base::Process();
  }

  // Allow the renderer and gpu processes to access the log file.
  if (type_str == switches::kRendererProcess ||
      type_str == switches::kGpuProcess) {
    if (logging::IsLoggingToFileEnabled()) {
      DCHECK(base::FilePath(logging::GetLogFileFullPath()).IsAbsolute());
      policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                      sandbox::TargetPolicy::FILES_ALLOW_ANY,
                      logging::GetLogFileFullPath().c_str());
    }
  }

#if !defined(OFFICIAL_BUILD)
  // If stdout/stderr point to a Windows console, these calls will
  // have no effect.
  policy->SetStdoutHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  policy->SetStderrHandle(GetStdHandle(STD_ERROR_HANDLE));
#endif

  if (!delegate->PreSpawnTarget(policy))
    return base::Process();

  TRACE_EVENT_BEGIN0("startup", "StartProcessWithAccess::LAUNCHPROCESS");

  PROCESS_INFORMATION temp_process_info = {};
  sandbox::ResultCode result = g_broker_services->SpawnTarget(
      cmd_line->GetProgram().value().c_str(),
      cmd_line->GetCommandLineString().c_str(), policy, &temp_process_info);
  DWORD last_error = ::GetLastError();
  base::win::ScopedProcessInformation target(temp_process_info);

  TRACE_EVENT_END0("startup", "StartProcessWithAccess::LAUNCHPROCESS");

  if (sandbox::SBOX_ALL_OK != result) {
    if (result == sandbox::SBOX_ERROR_GENERIC)
      DPLOG(ERROR) << "Failed to launch process";
    else if (result == sandbox::SBOX_ERROR_CREATE_PROCESS) {
      // TODO(shrikant): Remove this special case handling after determining
      // cause for lowbox/createprocess errors.
      sandbox::PolicyBase* policy_base =
          static_cast<sandbox::PolicyBase*>(policy);
      UMA_HISTOGRAM_SPARSE_SLOWLY(policy_base->GetLowBoxSid() ?
                                      "Process.Sandbox.Lowbox.Launch.Error" :
                                      "Process.Sandbox.Launch.Error",
                                  last_error);
    } else
      DLOG(ERROR) << "Failed to launch process. Error: " << result;

    return base::Process();
  }

  delegate->PostSpawnTarget(target.process_handle());

  CHECK(ResumeThread(target.thread_handle()) != static_cast<DWORD>(-1));
  return base::Process(target.TakeProcessHandle());
}

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
                                target_process.Get(), target_handle,
                                desired_access, FALSE, options);
  }

  return false;
}

bool BrokerAddTargetPeer(HANDLE peer_process) {
  return g_broker_services->AddTargetPeer(peer_process) == sandbox::SBOX_ALL_OK;
}

}  // namespace content
