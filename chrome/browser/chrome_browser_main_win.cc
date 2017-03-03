// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_win.h"

#include <shellapi.h>
#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include <algorithm>
#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/win/pe_image.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_event_sink_impl_win.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/install_verification/win/install_verification.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/browser/win/chrome_elf_init.h"
#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/win/message_box_win.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/gfx/switches.h"
#include "ui/strings/grit/app_locale_settings.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/google/did_run_updater_win.h"
#endif

namespace {

typedef HRESULT (STDAPICALLTYPE* RegisterApplicationRestartProc)(
    const wchar_t* command_line,
    DWORD flags);

void InitializeWindowProcExceptions() {
  // Get the breakpad pointer from chrome.exe
  base::win::WinProcExceptionFilter exception_filter =
      reinterpret_cast<base::win::WinProcExceptionFilter>(::GetProcAddress(
          ::GetModuleHandle(chrome::kChromeElfDllName), "CrashForException"));
  CHECK(exception_filter);
  exception_filter = base::win::SetWinProcExceptionFilter(exception_filter);
  DCHECK(!exception_filter);
}

// gfx::Font callbacks
void AdjustUIFont(LOGFONT* logfont) {
  l10n_util::AdjustUIFont(logfont);
}

int GetMinimumFontSize() {
  int min_font_size;
  base::StringToInt(l10n_util::GetStringUTF16(IDS_MINIMUM_UI_FONT_SIZE),
                    &min_font_size);
  return min_font_size;
}

class TranslationDelegate : public installer::TranslationDelegate {
 public:
  base::string16 GetLocalizedString(int installer_string_id) override;
};

void DetectFaultTolerantHeap() {
  enum FTHFlags {
    FTH_HKLM = 1,
    FTH_HKCU = 2,
    FTH_ACLAYERS_LOADED = 4,
    FTH_ACXTRNAL_LOADED = 8,
    FTH_FLAGS_COUNT = 16
  };

  // The Fault Tolerant Heap (FTH) is enabled on some customer machines and is
  // affecting their performance. We need to know how many machines are
  // affected in order to decide what to do.

  // The main way that the FTH is enabled is by having a value set in
  // HKLM\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers
  // whose name is the full path to the executable and whose data is the
  // string FaultTolerantHeap. Some documents suggest that this data may also
  // be found in HKCU. There have also been cases observed where this registry
  // key is set but the FTH is not enabled, so we also look for AcXtrnal.dll
  // and AcLayers.dll which are used to implement the FTH on Windows 7 and 8
  // respectively.

  // Get the module path so that we can look for it in the registry.
  wchar_t module_path[MAX_PATH];
  GetModuleFileName(NULL, module_path, ARRAYSIZE(module_path));
  // Force null-termination, necessary on Windows XP.
  module_path[ARRAYSIZE(module_path)-1] = 0;

  const wchar_t* const kRegPath = L"Software\\Microsoft\\Windows NT\\"
        L"CurrentVersion\\AppCompatFlags\\Layers";
  const wchar_t* const kFTHData = L"FaultTolerantHeap";
  // We always want to read from the 64-bit version of the registry if present,
  // since that is what the OS looks at, even for 32-bit processes.
  const DWORD kRegFlags = KEY_READ | KEY_WOW64_64KEY;

  base::win::RegKey FTH_HKLM_reg(HKEY_LOCAL_MACHINE, kRegPath, kRegFlags);
  FTHFlags detected = FTHFlags();
  base::string16 chrome_app_compat;
  if (FTH_HKLM_reg.ReadValue(module_path, &chrome_app_compat) == 0) {
    // This *usually* indicates that the fault tolerant heap is enabled.
    if (wcsicmp(chrome_app_compat.c_str(), kFTHData) == 0)
      detected = static_cast<FTHFlags>(detected | FTH_HKLM);
  }

  base::win::RegKey FTH_HKCU_reg(HKEY_CURRENT_USER, kRegPath, kRegFlags);
  if (FTH_HKCU_reg.ReadValue(module_path, &chrome_app_compat) == 0) {
    if (wcsicmp(chrome_app_compat.c_str(), kFTHData) == 0)
      detected = static_cast<FTHFlags>(detected | FTH_HKCU);
  }

  // Look for the DLLs used to implement the FTH and other compat hacks.
  if (GetModuleHandleW(L"AcLayers.dll") != NULL)
    detected = static_cast<FTHFlags>(detected | FTH_ACLAYERS_LOADED);
  if (GetModuleHandleW(L"AcXtrnal.dll") != NULL)
    detected = static_cast<FTHFlags>(detected | FTH_ACXTRNAL_LOADED);

  UMA_HISTOGRAM_ENUMERATION("FaultTolerantHeap", detected, FTH_FLAGS_COUNT);
}

// Helper function for getting the time date stamp associated with a module in
// this process.
uint32_t GetModuleTimeDateStamp(const void* module_load_address) {
  base::win::PEImage pe_image(module_load_address);
  return pe_image.GetNTHeaders()->FileHeader.TimeDateStamp;
}

// Used as the callback for ModuleWatcher events in this process. Dispatches
// them to the ModuleDatabase.
void OnModuleEvent(uint32_t process_id,
                   uint64_t creation_time,
                   const ModuleWatcher::ModuleEvent& event) {
  auto* module_database = ModuleDatabase::GetInstance();
  uintptr_t load_address =
      reinterpret_cast<uintptr_t>(event.module_load_address);

  switch (event.event_type) {
    case mojom::ModuleEventType::MODULE_ALREADY_LOADED:
    case mojom::ModuleEventType::MODULE_LOADED: {
      module_database->OnModuleLoad(
          process_id, creation_time, event.module_path, event.module_size,
          GetModuleTimeDateStamp(event.module_load_address), load_address);
      return;
    }

    case mojom::ModuleEventType::MODULE_UNLOADED: {
      module_database->OnModuleUnload(process_id, creation_time, load_address);
      return;
    }
  }
}

// Helper function for initializing the module database subsystem. Populates
// the provided |module_watcher|.
void SetupModuleDatabase(std::unique_ptr<ModuleWatcher>* module_watcher) {
  uint64_t creation_time = 0;
  ModuleEventSinkImpl::GetProcessCreationTime(::GetCurrentProcess(),
                                              &creation_time);
  ModuleDatabase::SetInstance(base::MakeUnique<ModuleDatabase>(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI)));
  auto* module_database = ModuleDatabase::GetInstance();
  uint32_t process_id = ::GetCurrentProcessId();

  // The ModuleWatcher will immediately start emitting module events, but the
  // ModuleDatabase expects an OnProcessStarted event prior to that. For child
  // processes this is handled via the ModuleEventSinkImpl. For the browser
  // process a manual notification is sent before wiring up the ModuleWatcher.
  module_database->OnProcessStarted(process_id, creation_time,
                                    content::PROCESS_TYPE_BROWSER);
  *module_watcher = ModuleWatcher::Create(
      base::Bind(&OnModuleEvent, process_id, creation_time));
}

}  // namespace

void ShowCloseBrowserFirstMessageBox() {
  int message_id = IDS_UNINSTALL_CLOSE_APP;
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      (shell_integration::GetDefaultBrowser() ==
       shell_integration::IS_DEFAULT)) {
    message_id = IDS_UNINSTALL_CLOSE_APP_IMMERSIVE;
  }
  chrome::ShowWarningMessageBox(NULL,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                                l10n_util::GetStringUTF16(message_id));
}

int DoUninstallTasks(bool chrome_still_running) {
  // We want to show a warning to user (and exit) if Chrome is already running
  // *before* we show the uninstall confirmation dialog box. But while the
  // uninstall confirmation dialog is up, user might start Chrome, so we
  // check once again after user acknowledges Uninstall dialog.
  if (chrome_still_running) {
    ShowCloseBrowserFirstMessageBox();
    return chrome::RESULT_CODE_UNINSTALL_CHROME_ALIVE;
  }
  int result = chrome::ShowUninstallBrowserPrompt();
  if (browser_util::IsBrowserAlreadyRunning()) {
    ShowCloseBrowserFirstMessageBox();
    return chrome::RESULT_CODE_UNINSTALL_CHROME_ALIVE;
  }

  if (result != chrome::RESULT_CODE_UNINSTALL_USER_CANCEL) {
    // The following actions are just best effort.
    // TODO(gab): Look into removing this code which is now redundant with the
    // work done by setup.exe on uninstall.
    VLOG(1) << "Executing uninstall actions";
    base::FilePath chrome_exe;
    if (PathService::Get(base::FILE_EXE, &chrome_exe)) {
      ShellUtil::ShortcutLocation user_shortcut_locations[] = {
          ShellUtil::SHORTCUT_LOCATION_DESKTOP,
          ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
          ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
      };
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      for (size_t i = 0; i < arraysize(user_shortcut_locations); ++i) {
        if (!ShellUtil::RemoveShortcuts(user_shortcut_locations[i], dist,
                ShellUtil::CURRENT_USER, chrome_exe)) {
          VLOG(1) << "Failed to delete shortcut at location "
                  << user_shortcut_locations[i];
        }
      }
    } else {
      NOTREACHED();
    }
  }
  return result;
}

// ChromeBrowserMainPartsWin ---------------------------------------------------

ChromeBrowserMainPartsWin::ChromeBrowserMainPartsWin(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
}

ChromeBrowserMainPartsWin::~ChromeBrowserMainPartsWin() {
}

void ChromeBrowserMainPartsWin::ToolkitInitialized() {
  ChromeBrowserMainParts::ToolkitInitialized();
  gfx::PlatformFontWin::adjust_font_callback = &AdjustUIFont;
  gfx::PlatformFontWin::get_minimum_font_size_callback = &GetMinimumFontSize;
  ui::CursorLoaderWin::SetCursorResourceModule(chrome::kBrowserResourcesDll);
}

void ChromeBrowserMainPartsWin::PreMainMessageLoopStart() {
  // installer_util references strings that are normally compiled into
  // setup.exe.  In Chrome, these strings are in the locale files.
  SetupInstallerUtilStrings();

  ChromeBrowserMainParts::PreMainMessageLoopStart();
  if (!parameters().ui_task) {
    // Make sure that we know how to handle exceptions from the message loop.
    InitializeWindowProcExceptions();
  }
}

int ChromeBrowserMainPartsWin::PreCreateThreads() {
  // Record whether the machine is enterprise managed in a crash key. This will
  // be used to better identify whether crashes are from enterprise users.
  base::debug::SetCrashKeyValue(
      crash_keys::kIsEnterpriseManaged,
      base::win::IsEnterpriseManaged() ? "yes" : "no");

  int rv = ChromeBrowserMainParts::PreCreateThreads();

  // TODO(viettrungluu): why don't we run this earlier?
  if (!parsed_command_line().HasSwitch(switches::kNoErrorDialogs) &&
      base::win::GetVersion() < base::win::VERSION_XP) {
    chrome::ShowWarningMessageBox(
        NULL, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
        l10n_util::GetStringUTF16(IDS_UNSUPPORTED_OS_PRE_WIN_XP));
  }

  return rv;
}

void ChromeBrowserMainPartsWin::ShowMissingLocaleMessageBox() {
  ui::MessageBox(NULL,
                 base::ASCIIToUTF16(chrome_browser::kMissingLocaleDataMessage),
                 base::ASCIIToUTF16(chrome_browser::kMissingLocaleDataTitle),
                 MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void ChromeBrowserMainPartsWin::PostProfileInit() {
  ChromeBrowserMainParts::PostProfileInit();

  // TODO(kulshin): remove this cleanup code in 2017. http://crbug.com/603718
  // Attempt to delete the font cache and ignore any errors.
  base::FilePath path(
      profile()->GetPath().AppendASCII("ChromeDWriteFontCache"));
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, content::BrowserThread::GetTaskRunnerForThread(
                     content::BrowserThread::FILE),
      base::Bind(base::IgnoreResult(&base::DeleteFile), path, false));

  // Create the module database and hook up the in-process module watcher. This
  // needs to be done before any child processes are initialized as the
  // ModuleDatabase is an endpoint for IPC from child processes.
  if (base::FeatureList::IsEnabled(features::kModuleDatabase))
    SetupModuleDatabase(&module_watcher_);
}

void ChromeBrowserMainPartsWin::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  UMA_HISTOGRAM_BOOLEAN("Windows.Tablet", base::win::IsTabletDevice(nullptr));

  // Set up a task to verify installed modules in the current process.
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, content::BrowserThread::GetBlockingPool(),
      base::Bind(&VerifyInstallation));

  InitializeChromeElf();

  if (base::FeatureList::IsEnabled(safe_browsing::kSettingsResetPrompt)) {
    content::BrowserThread::PostAfterStartupTask(
        FROM_HERE,
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::UI),
        base::Bind(safe_browsing::MaybeShowSettingsResetPromptWithDelay));
  }

  // Record UMA data about whether the fault-tolerant heap is enabled.
  // Use a delayed task to minimize the impact on startup time.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DetectFaultTolerantHeap),
      base::TimeDelta::FromMinutes(1));
}

// static
void ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment(
    const base::CommandLine& parsed_command_line) {
  // Clear this var so child processes don't show the dialog by default.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->UnSetVar(env_vars::kShowRestart);

  // For non-interactive tests we don't restart on crash.
  if (env->HasVar(env_vars::kHeadless))
    return;

  // If the known command-line test options are used we don't create the
  // environment block which means we don't get the restart dialog.
  if (parsed_command_line.HasSwitch(switches::kBrowserCrashTest) ||
      parsed_command_line.HasSwitch(switches::kNoErrorDialogs))
    return;

  // The encoding we use for the info is "title|context|direction" where
  // direction is either env_vars::kRtlLocale or env_vars::kLtrLocale depending
  // on the current locale.
  base::string16 dlg_strings(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_TITLE));
  dlg_strings.push_back('|');
  base::string16 adjusted_string(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_CONTENT));
  base::i18n::AdjustStringForLocaleDirection(&adjusted_string);
  dlg_strings.append(adjusted_string);
  dlg_strings.push_back('|');
  dlg_strings.append(base::ASCIIToUTF16(
      base::i18n::IsRTL() ? env_vars::kRtlLocale : env_vars::kLtrLocale));

  env->SetVar(env_vars::kRestartInfo, base::UTF16ToUTF8(dlg_strings));
}

// static
void ChromeBrowserMainPartsWin::RegisterApplicationRestart(
    const base::CommandLine& parsed_command_line) {
  DCHECK(base::win::GetVersion() >= base::win::VERSION_VISTA);
  base::ScopedNativeLibrary library(base::FilePath(L"kernel32.dll"));
  // Get the function pointer for RegisterApplicationRestart.
  RegisterApplicationRestartProc register_application_restart =
      reinterpret_cast<RegisterApplicationRestartProc>(
          library.GetFunctionPointer("RegisterApplicationRestart"));
  if (!register_application_restart) {
    LOG(WARNING) << "Cannot find RegisterApplicationRestart in kernel32.dll";
    return;
  }
  // The Windows Restart Manager expects a string of command line flags only,
  // without the program.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendArguments(parsed_command_line, false);
  if (!command_line.HasSwitch(switches::kRestoreLastSession))
    command_line.AppendSwitch(switches::kRestoreLastSession);

  // Restart Chrome if the computer is restarted as the result of an update.
  // This could be extended to handle crashes, hangs, and patches.
  HRESULT hr = register_application_restart(
      command_line.GetCommandLineString().c_str(),
      RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH);
  if (FAILED(hr)) {
    if (hr == E_INVALIDARG) {
      LOG(WARNING) << "Command line too long for RegisterApplicationRestart";
    } else {
      NOTREACHED() << "RegisterApplicationRestart failed. hr: " << hr <<
                      ", command_line: " << command_line.GetCommandLineString();
    }
  }
}

// static
int ChromeBrowserMainPartsWin::HandleIconsCommands(
    const base::CommandLine& parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kHideIcons)) {
    base::string16 cp_applet;
    base::win::Version version = base::win::GetVersion();
    if (version >= base::win::VERSION_VISTA) {
      cp_applet.assign(L"Programs and Features");  // Windows Vista and later.
    } else if (version >= base::win::VERSION_XP) {
      cp_applet.assign(L"Add/Remove Programs");  // Windows XP.
    } else {
      return chrome::RESULT_CODE_UNSUPPORTED_PARAM;  // Not supported
    }

    const base::string16 msg =
        l10n_util::GetStringFUTF16(IDS_HIDE_ICONS_NOT_SUPPORTED, cp_applet);
    const base::string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    const UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST;
    if (IDOK == ui::MessageBox(NULL, msg, caption, flags))
      ShellExecute(NULL, NULL, L"appwiz.cpl", NULL, NULL, SW_SHOWNORMAL);

    // Exit as we are not launching the browser.
    return content::RESULT_CODE_NORMAL_EXIT;
  }
  // We don't hide icons so we shouldn't do anything special to show them
  return chrome::RESULT_CODE_UNSUPPORTED_PARAM;
}

// static
bool ChromeBrowserMainPartsWin::CheckMachineLevelInstall() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::Version version;
  InstallUtil::GetChromeVersion(dist, true, &version);
  if (version.IsValid()) {
    base::FilePath exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    std::wstring exe = exe_path.value();
    base::FilePath user_exe_path(installer::GetChromeInstallPath(false, dist));
    if (base::FilePath::CompareEqualIgnoreCase(exe, user_exe_path.value())) {
      base::CommandLine uninstall_cmd(
          InstallUtil::GetChromeUninstallCmd(false));
      if (!uninstall_cmd.GetProgram().empty()) {
        uninstall_cmd.AppendSwitch(installer::switches::kSelfDestruct);
        uninstall_cmd.AppendSwitch(installer::switches::kForceUninstall);
        uninstall_cmd.AppendSwitch(
            installer::switches::kDoNotRemoveSharedItems);

        // Trigger Active Setup for the system-level Chrome to make sure
        // per-user shortcuts to the system-level Chrome are created. Skip this
        // if the system-level Chrome will undergo first run anyway, as Active
        // Setup is triggered on system-level Chrome's first run.
        // TODO(gab): Instead of having callers of Active Setup think about
        // other callers, have Active Setup itself register when it ran and
        // no-op otherwise (http://crbug.com/346843).
        if (!first_run::IsChromeFirstRun())
          uninstall_cmd.AppendSwitch(installer::switches::kTriggerActiveSetup);

        const base::FilePath setup_exe(uninstall_cmd.GetProgram());
        const base::string16 params(uninstall_cmd.GetArgumentsString());

        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOASYNC;
        sei.nShow = SW_SHOWNORMAL;
        sei.lpFile = setup_exe.value().c_str();
        sei.lpParameters = params.c_str();

        if (!::ShellExecuteEx(&sei))
          DPCHECK(false);
      }
      return true;
    }
  }
  return false;
}

base::string16 TranslationDelegate::GetLocalizedString(
    int installer_string_id) {
  int resource_id = 0;
  switch (installer_string_id) {
  // HANDLE_STRING is used by the DO_INSTALLER_STRING_MAPPING macro which is in
  // the generated header installer_util_strings.h.
#define HANDLE_STRING(base_id, chrome_id) \
  case base_id: \
    resource_id = chrome_id; \
    break;
  DO_INSTALLER_STRING_MAPPING
#undef HANDLE_STRING
  default:
    NOTREACHED();
  }
  if (resource_id)
    return l10n_util::GetStringUTF16(resource_id);
  return base::string16();
}

// static
void ChromeBrowserMainPartsWin::SetupInstallerUtilStrings() {
  CR_DEFINE_STATIC_LOCAL(TranslationDelegate, delegate, ());
  installer::SetTranslationDelegate(&delegate);
}
