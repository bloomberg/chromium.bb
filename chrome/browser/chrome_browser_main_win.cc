// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_win.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/metro.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/browser_util_win.h"
#include "chrome/browser/chrome_elf_init_win.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/install_verification/win/install_verification.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/terminate_on_heap_corruption_experiment_win.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/main_function_params.h"
#include "grit/app_locale_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "installer_util_strings/installer_util_strings.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/win/message_box_win.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/gfx/switches.h"

namespace {

typedef HRESULT (STDAPICALLTYPE* RegisterApplicationRestartProc)(
    const wchar_t* command_line,
    DWORD flags);

void InitializeWindowProcExceptions() {
  // Get the breakpad pointer from chrome.exe
  base::win::WinProcExceptionFilter exception_filter =
      reinterpret_cast<base::win::WinProcExceptionFilter>(
          ::GetProcAddress(::GetModuleHandle(
                               chrome::kBrowserProcessExecutableName),
                           "CrashForException"));
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
  virtual base::string16 GetLocalizedString(int installer_string_id) OVERRIDE;
};

bool IsSafeModeStart() {
  return ::GetEnvironmentVariableA(chrome::kSafeModeEnvVar, NULL, 0) != 0;
}

}  // namespace

void ShowCloseBrowserFirstMessageBox() {
  int message_id = IDS_UNINSTALL_CLOSE_APP;
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      (ShellIntegration::GetDefaultBrowser() == ShellIntegration::IS_DEFAULT)) {
    message_id = IDS_UNINSTALL_CLOSE_APP_IMMERSIVE;
  }
  chrome::ShowMessageBox(NULL,
                         l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                         l10n_util::GetStringUTF16(message_id),
                         chrome::MESSAGE_BOX_TYPE_WARNING);
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
        ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
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

void MaybeEnableHighResolutionTimeEverywhere() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  bool user_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableHighResolutionTime);
  if (user_enabled || channel == chrome::VersionInfo::CHANNEL_CANARY) {
    bool is_enabled = base::TimeTicks::SetNowIsHighResNowIfSupported();
    if (is_enabled && !user_enabled) {
      // Ensure that all of the renderers will enable it too.
      CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableHighResolutionTime);
    }
  }
}

// ChromeBrowserMainPartsWin ---------------------------------------------------

ChromeBrowserMainPartsWin::ChromeBrowserMainPartsWin(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
  MaybeEnableHighResolutionTimeEverywhere();
  if (base::win::IsMetroProcess()) {
    typedef const wchar_t* (*GetMetroSwitches)(void);
    GetMetroSwitches metro_switches_proc = reinterpret_cast<GetMetroSwitches>(
        GetProcAddress(base::win::GetMetroModule(),
                       "GetMetroCommandLineSwitches"));
    if (metro_switches_proc) {
      base::string16 metro_switches = (*metro_switches_proc)();
      if (!metro_switches.empty()) {
        CommandLine extra_switches(CommandLine::NO_PROGRAM);
        extra_switches.ParseFromString(metro_switches);
        CommandLine::ForCurrentProcess()->AppendArguments(extra_switches,
                                                          false);
      }
    }
  }
}

ChromeBrowserMainPartsWin::~ChromeBrowserMainPartsWin() {
}

void ChromeBrowserMainPartsWin::ToolkitInitialized() {
  ChromeBrowserMainParts::ToolkitInitialized();
  gfx::PlatformFontWin::adjust_font_callback = &AdjustUIFont;
  gfx::PlatformFontWin::get_minimum_font_size_callback = &GetMinimumFontSize;
#if defined(USE_AURA)
  ui::CursorLoaderWin::SetCursorResourceModule(chrome::kBrowserResourcesDll);
#endif
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

  IncognitoModePrefs::InitializePlatformParentalControls();
}

int ChromeBrowserMainPartsWin::PreCreateThreads() {
  int rv = ChromeBrowserMainParts::PreCreateThreads();

  if (IsSafeModeStart()) {
    // TODO(cpu): disable other troublesome features for safe mode.
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGpu);
  }
  // TODO(viettrungluu): why don't we run this earlier?
  if (!parsed_command_line().HasSwitch(switches::kNoErrorDialogs) &&
      base::win::GetVersion() < base::win::VERSION_XP) {
    chrome::ShowMessageBox(NULL,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
        l10n_util::GetStringUTF16(IDS_UNSUPPORTED_OS_PRE_WIN_XP),
        chrome::MESSAGE_BOX_TYPE_WARNING);
  }

  return rv;
}

void ChromeBrowserMainPartsWin::ShowMissingLocaleMessageBox() {
  ui::MessageBox(NULL,
                 base::ASCIIToUTF16(chrome_browser::kMissingLocaleDataMessage),
                 base::ASCIIToUTF16(chrome_browser::kMissingLocaleDataTitle),
                 MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void ChromeBrowserMainPartsWin::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  UMA_HISTOGRAM_BOOLEAN("Windows.Tablet", base::win::IsTabletDevice());

  // Set up a task to verify installed modules in the current process. Use a
  // delay to reduce the impact on startup time.
  content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI)->PostDelayedTask(
          FROM_HERE,
          base::Bind(&VerifyInstallation),
          base::TimeDelta::FromSeconds(45));

  InitializeChromeElf();

  // TODO(erikwright): Remove this and the implementation of the experiment by
  // September 2014.
  InitializeDisableTerminateOnHeapCorruptionExperiment();
}

// static
void ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment(
    const CommandLine& parsed_command_line) {
  // Clear this var so child processes don't show the dialog by default.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->UnSetVar(env_vars::kShowRestart);

  // For non-interactive tests we don't restart on crash.
  if (env->HasVar(env_vars::kHeadless))
    return;

  // If the known command-line test options are used we don't create the
  // environment block which means we don't get the restart dialog.
  if (parsed_command_line.HasSwitch(switches::kBrowserCrashTest) ||
      parsed_command_line.HasSwitch(switches::kBrowserAssertTest) ||
      parsed_command_line.HasSwitch(switches::kNoErrorDialogs))
    return;

  // The encoding we use for the info is "title|context|direction" where
  // direction is either env_vars::kRtlLocale or env_vars::kLtrLocale depending
  // on the current locale.
  base::string16 dlg_strings(l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_TITLE));
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
    const CommandLine& parsed_command_line) {
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
  CommandLine command_line(CommandLine::NO_PROGRAM);
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
    const CommandLine& parsed_command_line) {
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
  // TODO(tommi): Check if using the default distribution is always the right
  // thing to do.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  Version version;
  InstallUtil::GetChromeVersion(dist, true, &version);
  if (version.IsValid()) {
    base::FilePath exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    std::wstring exe = exe_path.value();
    base::FilePath user_exe_path(installer::GetChromeInstallPath(false, dist));
    if (base::FilePath::CompareEqualIgnoreCase(exe, user_exe_path.value())) {
      bool is_metro = base::win::IsMetroProcess();
      if (!is_metro) {
        // The dialog cannot be shown in Win8 Metro as doing so hangs Chrome on
        // an invisible dialog.
        // TODO (gab): Get rid of this dialog altogether and auto-launch
        // system-level Chrome instead.
        const base::string16 text =
            l10n_util::GetStringUTF16(IDS_MACHINE_LEVEL_INSTALL_CONFLICT);
        const base::string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
        const UINT flags = MB_OK | MB_ICONERROR | MB_TOPMOST;
        ui::MessageBox(NULL, text, caption, flags);
      }
      CommandLine uninstall_cmd(
          InstallUtil::GetChromeUninstallCmd(false, dist->GetType()));
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
        // On Windows 8 SEE_MASK_FLAG_LOG_USAGE is necessary to guarantee we
        // flip to the Desktop when launching.
        if (is_metro)
          sei.fMask |= SEE_MASK_FLAG_LOG_USAGE;

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
