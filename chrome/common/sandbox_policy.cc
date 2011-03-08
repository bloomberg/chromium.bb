// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_policy.h"

#include <string>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "sandbox/src/sandbox.h"

static sandbox::BrokerServices* g_broker_services = NULL;

namespace {

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the renderer.
const wchar_t* const kTroublesomeDlls[] = {
  L"adialhk.dll",                 // Kaspersky Internet Security.
  L"acpiz.dll",                   // Unknown.
  L"avgrsstx.dll",                // AVG 8.
  L"btkeyind.dll",                // Widcomm Bluetooth.
  L"cmcsyshk.dll",                // CMC Internet Security.
  L"cutildll.dll",                // Citrix
  L"dockshellhook.dll",           // Stardock Objectdock.
  L"emulatetermsrv.dll",          // Citrix
  L"GoogleDesktopNetwork3.DLL",   // Google Desktop Search v5.
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
  L"npdivx32.dll",                // DivX.
  L"npggNT.des",                  // GameGuard 2008.
  L"npggNT.dll",                  // GameGuard (older).
  L"oawatch.dll",                 // Online Armor.
  L"pavhook.dll",                 // Panda Internet Security.
  L"pavshook.dll",                // Panda Antivirus.
  L"pctavhook.dll",               // PC Tools Antivirus.
  L"pctgmhk.dll",                 // PC Tools Spyware Doctor.
  L"prntrack.dll",                // Pharos Systems.
  L"radhslib.dll",                // Radiant Naomi Internet Filter.
  L"radprlib.dll",                // Radiant Naomi Internet Filter.
  L"rlhook.dll",                  // Trustware Bufferzone.
  L"rpchromebrowserrecordhelper.dll",  // RealPlayer.
  L"r3hook.dll",                  // Kaspersky Internet Security.
  L"sahook.dll",                  // McAfee Site Advisor.
  L"sbrige.dll",                  // Unknown.
  L"sc2hook.dll",                 // Supercopier 2.
  L"sehook20.dll",                // Citrix
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

enum PluginPolicyCategory {
  PLUGIN_GROUP_TRUSTED,
  PLUGIN_GROUP_UNTRUSTED,
};

// Returns the policy category for the plugin dll.
PluginPolicyCategory GetPolicyCategoryForPlugin(
    const std::wstring& dll,
    const std::wstring& list) {
  std::wstring filename = FilePath(dll).BaseName().value();
  std::wstring plugin_dll = StringToLowerASCII(filename);
  std::wstring trusted_plugins = StringToLowerASCII(list);

  size_t pos = 0;
  size_t end_item = 0;
  while (end_item != std::wstring::npos) {
    end_item = list.find(L",", pos);

    size_t size_item = (end_item == std::wstring::npos) ? end_item :
                                                          end_item - pos;
    std::wstring item = list.substr(pos, size_item);
    if (!item.empty() && item == plugin_dll)
      return PLUGIN_GROUP_TRUSTED;

    pos = end_item + 1;
  }

  return PLUGIN_GROUP_UNTRUSTED;
}

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

// Adds policy rules for unloaded the known dlls that cause chrome to crash.
// Eviction of injected DLLs is done by the sandbox so that the injected module
// does not get a chance to execute any code.
void AddDllEvictionPolicy(sandbox::TargetPolicy* policy) {
  for (int ix = 0; ix != arraysize(kTroublesomeDlls); ++ix) {
    // To minimize the list we only add an unload policy if the dll is also
    // loaded in this process. All the injected dlls of interest do this.
    if (::GetModuleHandleW(kTroublesomeDlls[ix])) {
      VLOG(1) << "dll to unload found: " << kTroublesomeDlls[ix];
      policy->AddDllToUnload(kTroublesomeDlls[ix]);
    }
  }
}

// Adds the generic policy rules to a sandbox TargetPolicy.
bool AddGenericPolicy(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;

  // Add the policy for the pipes
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.nacl.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Add the policy for debug message only in debug
#ifndef NDEBUG
  FilePath app_dir;
  if (!PathService::Get(chrome::DIR_APP, &app_dir))
    return false;

  wchar_t long_path_buf[MAX_PATH];
  DWORD long_path_return_value = GetLongPathName(app_dir.value().c_str(),
                                                 long_path_buf,
                                                 MAX_PATH);
  if (long_path_return_value == 0 || long_path_return_value >= MAX_PATH)
    return false;

  string16 debug_message(long_path_buf);
  file_util::AppendToPath(&debug_message, L"debug_message.exe");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                           sandbox::TargetPolicy::PROCESS_MIN_EXEC,
                           debug_message.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif  // NDEBUG

  return true;
}

// Creates a sandbox without any restriction.
bool ApplyPolicyForTrustedPlugin(sandbox::TargetPolicy* policy) {
  policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
  policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  return true;
}

// Creates a sandbox with the plugin running in a restricted environment.
// Only the "Users" and "Everyone" groups are enabled in the token. The User SID
// is disabled.
bool ApplyPolicyForUntrustedPlugin(sandbox::TargetPolicy* policy) {
  policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);

  sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // On 2003/Vista the initial token has to be restricted if the main token
    // is restricted.
    initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
  }
  policy->SetTokenLevel(initial_token, sandbox::USER_LIMITED);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  if (!AddDirectory(base::DIR_TEMP, NULL, true,
                    sandbox::TargetPolicy::FILES_ALLOW_ANY, policy))
    return false;

  if (!AddDirectory(base::DIR_IE_INTERNET_CACHE, NULL, true,
                    sandbox::TargetPolicy::FILES_ALLOW_ANY, policy))
    return false;

  if (!AddDirectory(base::DIR_APP_DATA, NULL, true,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    policy))
    return false;

  if (!AddDirectory(base::DIR_PROFILE, NULL, false,  /*not recursive*/
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    policy))
    return false;

  if (!AddDirectory(base::DIR_APP_DATA, L"Adobe", true,
                    sandbox::TargetPolicy::FILES_ALLOW_ANY,
                    policy))
    return false;

  if (!AddDirectory(base::DIR_APP_DATA, L"Macromedia", true,
                    sandbox::TargetPolicy::FILES_ALLOW_ANY,
                    policy))
    return false;

  if (!AddDirectory(base::DIR_LOCAL_APP_DATA, NULL, true,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    policy))
    return false;

  if (!AddKeyAndSubkeys(L"HKEY_CURRENT_USER\\SOFTWARE\\ADOBE",
                        sandbox::TargetPolicy::REG_ALLOW_ANY,
                        policy))
    return false;

  if (!AddKeyAndSubkeys(L"HKEY_CURRENT_USER\\SOFTWARE\\MACROMEDIA",
                        sandbox::TargetPolicy::REG_ALLOW_ANY,
                        policy))
    return false;

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    if (!AddKeyAndSubkeys(L"HKEY_CURRENT_USER\\SOFTWARE\\AppDataLow",
                          sandbox::TargetPolicy::REG_ALLOW_ANY,
                          policy))
      return false;

    if (!AddDirectory(base::DIR_LOCAL_APP_DATA_LOW, NULL, true,
                      sandbox::TargetPolicy::FILES_ALLOW_ANY,
                      policy))
      return false;

    // DIR_APP_DATA is AppData\Roaming, but Adobe needs to do a directory
    // listing in AppData directly, so we add a non-recursive policy for
    // AppData itself.
    if (!AddDirectory(base::DIR_APP_DATA, L"..", false,
                      sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                      policy))
      return false;
  }

  return true;
}

// Launches the privileged flash broker, used when flash is sandboxed.
// The broker is the same flash dll, except that it uses a different
// entrypoint (BrokerMain) and it is hosted in windows' generic surrogate
// process rundll32. After launching the broker we need to pass to
// the flash plugin the process id of the broker via the command line
// using --flash-broker=pid.
// More info about rundll32 at http://support.microsoft.com/kb/164787.
bool LoadFlashBroker(const FilePath& plugin_path, CommandLine* cmd_line) {
  FilePath rundll;
  if (!PathService::Get(base::DIR_SYSTEM, &rundll))
    return false;
  rundll = rundll.AppendASCII("rundll32.exe");
  // Rundll32 cannot handle paths with spaces, so we use the short path.
  wchar_t short_path[MAX_PATH];
  if (0 == ::GetShortPathNameW(plugin_path.value().c_str(),
                               short_path, arraysize(short_path)))
    return false;
  // Here is the kicker, if the user has disabled 8.3 (short path) support
  // on the volume GetShortPathNameW does not fail but simply returns the
  // input path. In this case if the path had any spaces then rundll32 will
  // incorrectly interpret its parameters. So we quote the path, even though
  // the kb/164787 says you should not.
  std::wstring cmd_final =
      base::StringPrintf(L"%ls \"%ls\",BrokerMain browser=chrome",
                         rundll.value().c_str(),
                         short_path);
  base::ProcessHandle process;
  if (!base::LaunchApp(cmd_final, false, true, &process))
    return false;

  cmd_line->AppendSwitchASCII("flash-broker",
                              base::Int64ToString(::GetProcessId(process)));

  // The flash broker, unders some circumstances can linger beyond the lifetime
  // of the flash player, so we put it in a job object, when the browser
  // terminates the job object is destroyed (by the OS) and the flash broker
  // is terminated.
  HANDLE job = ::CreateJobObjectW(NULL, NULL);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits = {0};
  job_limits.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (::SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                &job_limits, sizeof(job_limits))) {
    ::AssignProcessToJobObject(job, process);
    // Yes, we are leaking the object here. Read comment above.
  } else {
    ::CloseHandle(job);
    return false;
  }

  ::CloseHandle(process);
  return true;
}

// Creates a sandbox for the built-in flash plugin running in a restricted
// environment. This policy is in continual flux as flash changes
// capabilities. For more information see bug 50796.
bool ApplyPolicyForBuiltInFlashPlugin(sandbox::TargetPolicy* policy) {
  policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
  // Vista and Win7 get a weaker token but have low integrity.
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_INTERACTIVE);
    policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  } else {
    policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                          sandbox::USER_LIMITED);

    if (!AddKeyAndSubkeys(L"HKEY_LOCAL_MACHINE\\SOFTWARE",
                          sandbox::TargetPolicy::REG_ALLOW_READONLY,
                          policy))
      return false;
    if (!AddKeyAndSubkeys(L"HKEY_LOCAL_MACHINE\\SYSTEM",
                          sandbox::TargetPolicy::REG_ALLOW_READONLY,
                          policy))
      return false;

    if (!AddKeyAndSubkeys(L"HKEY_CURRENT_USER\\SOFTWARE",
                          sandbox::TargetPolicy::REG_ALLOW_READONLY,
                          policy))
      return false;
  }

  AddDllEvictionPolicy(policy);
  return true;
}

// Returns true of the plugin specified in |cmd_line| is the built-in
// flash plugin and optionally returns its full path in |flash_path|
bool IsBuiltInFlash(const CommandLine* cmd_line, FilePath* flash_path) {
  std::wstring plugin_dll = cmd_line->
      GetSwitchValueNative(switches::kPluginPath);

  FilePath builtin_flash;
  if (!PathService::Get(chrome::FILE_FLASH_PLUGIN, &builtin_flash))
    return false;

  FilePath plugin_path(plugin_dll);
  if (plugin_path != builtin_flash)
    return false;

  if (flash_path)
    *flash_path = plugin_path;
  return true;
}


// Adds the custom policy rules for a given plugin. |trusted_plugins| contains
// the comma separate list of plugin dll names that should not be sandboxed.
bool AddPolicyForPlugin(CommandLine* cmd_line,
                        sandbox::TargetPolicy* policy) {
  std::wstring plugin_dll = cmd_line->
      GetSwitchValueNative(switches::kPluginPath);
  std::wstring trusted_plugins = CommandLine::ForCurrentProcess()->
      GetSwitchValueNative(switches::kTrustedPlugins);
  // Add the policy for the pipes.
  sandbox::ResultCode result = sandbox::SBOX_ALL_OK;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK) {
    NOTREACHED();
    return false;
  }

  // The built-in flash gets a custom, more restricted sandbox.
  FilePath flash_path;
  if (IsBuiltInFlash(cmd_line, &flash_path)) {
    // Spawn the flash broker and apply sandbox policy.
    if (!LoadFlashBroker(flash_path, cmd_line)) {
      // Could not start the broker, use a very weak policy instead.
      DLOG(WARNING) << "Failed to start flash broker";
      return ApplyPolicyForTrustedPlugin(policy);
    }
    return ApplyPolicyForBuiltInFlashPlugin(policy);
  }

  PluginPolicyCategory policy_category =
      GetPolicyCategoryForPlugin(plugin_dll, trusted_plugins);

  switch (policy_category) {
    case PLUGIN_GROUP_TRUSTED:
      return ApplyPolicyForTrustedPlugin(policy);
    case PLUGIN_GROUP_UNTRUSTED:
      return ApplyPolicyForUntrustedPlugin(policy);
    default:
      NOTREACHED();
      break;
  }

  return false;
}

// For the GPU process we gotten as far as USER_LIMITED. The next level
// which is USER_RESTRICTED breaks both the DirectX backend and the OpenGL
// backend. Note that the GPU process is connected to the interactive
// desktop.
// TODO(cpu): Lock down the sandbox more if possible.
// TODO(apatrick): Use D3D9Ex to render windowless.
bool AddPolicyForGPU(CommandLine*, sandbox::TargetPolicy* policy) {
  policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);

  if (base::win::GetVersion() > base::win::VERSION_XP) {
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_LIMITED);
    policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  } else {
    policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                          sandbox::USER_LIMITED);
  }

  AddDllEvictionPolicy(policy);
  return true;
}

void AddPolicyForRenderer(sandbox::TargetPolicy* policy,
                          bool* on_sandbox_desktop) {
  policy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);

  sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
  if (base::win::GetVersion() > base::win::VERSION_XP) {
    // On 2003/Vista the initial token has to be restricted if the main
    // token is restricted.
    initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
  }

  policy->SetTokenLevel(initial_token, sandbox::USER_LOCKDOWN);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  bool use_winsta = !CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kDisableAltWinstation);

  if (sandbox::SBOX_ALL_OK ==  policy->SetAlternateDesktop(use_winsta)) {
    *on_sandbox_desktop = true;
  } else {
    *on_sandbox_desktop = false;
    DLOG(WARNING) << "Failed to apply desktop security to the renderer";
  }

  AddDllEvictionPolicy(policy);
}

}  // namespace

namespace sandbox {

void InitBrokerServices(sandbox::BrokerServices* broker_services) {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287166>.
  CHECK(broker_services);
  CHECK(!g_broker_services);
  broker_services->Init();
  g_broker_services = broker_services;
}

base::ProcessHandle StartProcessWithAccess(CommandLine* cmd_line,
                                           const FilePath& exposed_dir) {
  base::ProcessHandle process = 0;
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  ChildProcessInfo::ProcessType type;
  std::string type_str = cmd_line->GetSwitchValueASCII(switches::kProcessType);
  if (type_str == switches::kRendererProcess) {
    type = ChildProcessInfo::RENDER_PROCESS;
  } else if (type_str == switches::kExtensionProcess) {
    // Extensions are just renderers with another name.
    type = ChildProcessInfo::RENDER_PROCESS;
  } else if (type_str == switches::kPluginProcess) {
    type = ChildProcessInfo::PLUGIN_PROCESS;
  } else if (type_str == switches::kWorkerProcess) {
    type = ChildProcessInfo::WORKER_PROCESS;
  } else if (type_str == switches::kNaClLoaderProcess) {
    type = ChildProcessInfo::NACL_LOADER_PROCESS;
  } else if (type_str == switches::kUtilityProcess) {
    type = ChildProcessInfo::UTILITY_PROCESS;
  } else if (type_str == switches::kNaClBrokerProcess) {
    type = ChildProcessInfo::NACL_BROKER_PROCESS;
  } else if (type_str == switches::kGpuProcess) {
    type = ChildProcessInfo::GPU_PROCESS;
  } else if (type_str == switches::kPpapiPluginProcess) {
    type = ChildProcessInfo::PPAPI_PLUGIN_PROCESS;
  } else {
    NOTREACHED();
    return 0;
  }

  TRACE_EVENT_BEGIN("StartProcessWithAccess", 0, type_str);

  // To decide if the process is going to be sandboxed we have two cases.
  // First case: all process types except the nacl broker, and the plugin
  // process are sandboxed by default.
  bool in_sandbox =
      (type != ChildProcessInfo::NACL_BROKER_PROCESS) &&
      (type != ChildProcessInfo::PLUGIN_PROCESS);

  // Second case: If it is the plugin process then it depends on it being
  // the built-in flash, the user forcing plugins into sandbox or the
  // the user explicitly excluding flash from the sandbox.
  if (!in_sandbox && (type == ChildProcessInfo::PLUGIN_PROCESS)) {
      in_sandbox = browser_command_line.HasSwitch(switches::kSafePlugins) ||
          (IsBuiltInFlash(cmd_line, NULL) &&
           (base::win::GetVersion() > base::win::VERSION_XP) &&
           !browser_command_line.HasSwitch(switches::kDisableFlashSandbox));
  }

  // Third case: If it is the GPU process then it can be disabled by a
  // command line flag.
  if ((type == ChildProcessInfo::GPU_PROCESS) &&
      (browser_command_line.HasSwitch(switches::kDisableGpuSandbox))) {
    in_sandbox = false;
    VLOG(1) << "GPU sandbox is disabled";
  }

  if (browser_command_line.HasSwitch(switches::kNoSandbox)) {
    // The user has explicity opted-out from all sandboxing.
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

  if (!in_sandbox) {
    base::LaunchApp(*cmd_line, false, false, &process);
    return process;
  }

  sandbox::ResultCode result;
  PROCESS_INFORMATION target = {0};
  sandbox::TargetPolicy* policy = g_broker_services->CreatePolicy();

  bool on_sandbox_desktop = false;
  if (type == ChildProcessInfo::PLUGIN_PROCESS) {
    if (!AddPolicyForPlugin(cmd_line, policy))
      return 0;
  } else if (type == ChildProcessInfo::GPU_PROCESS) {
    if (!AddPolicyForGPU(cmd_line, policy))
      return 0;
  } else {
    AddPolicyForRenderer(policy, &on_sandbox_desktop);

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

  TRACE_EVENT_BEGIN("StartProcessWithAccess::LAUNCHPROCESS", 0, 0);

  result = g_broker_services->SpawnTarget(
      cmd_line->GetProgram().value().c_str(),
      cmd_line->command_line_string().c_str(),
      policy, &target);
  policy->Release();

  TRACE_EVENT_END("StartProcessWithAccess::LAUNCHPROCESS", 0, 0);

  if (sandbox::SBOX_ALL_OK != result)
    return 0;

  ResumeThread(target.hThread);
  CloseHandle(target.hThread);
  process = target.hProcess;

  // Help the process a little. It can't start the debugger by itself if
  // the process is in a sandbox.
  if (child_needs_help)
    base::debug::SpawnDebuggerOnProcess(target.dwProcessId);

  return process;
}

}  // namespace sandbox
