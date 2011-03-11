// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"
#include "chrome/browser/browser_main_win.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/rtl.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/ui/views/uninstall_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "content/common/main_function_params.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/winsock_init.h"
#include "net/socket/client_socket_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/message_box_win.h"
#include "views/focus/accelerator_handler.h"
#include "views/window/window.h"

namespace {
typedef HRESULT (STDAPICALLTYPE* RegisterApplicationRestartProc)(
    const wchar_t* command_line,
    DWORD flags);
}  // namespace

void DidEndMainMessageLoop() {
  OleUninitialize();
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
  DWORD len = ::GetEnvironmentVariableW(
      ASCIIToWide(env_vars::kNoOOBreakpad).c_str() , NULL, 0);
  metrics->RecordBreakpadRegistration((len == 0));
  metrics->RecordBreakpadHasDebugger(TRUE == ::IsDebuggerPresent());
}

void WarnAboutMinimumSystemRequirements() {
  if (base::win::GetVersion() < base::win::VERSION_XP) {
    // Display a warning message if the user is running chrome on Windows 2000.
    const string16 text =
        l10n_util::GetStringUTF16(IDS_UNSUPPORTED_OS_PRE_WIN_XP);
    const string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    ui::MessageBox(NULL, text, caption, MB_OK | MB_ICONWARNING | MB_TOPMOST);
  }
}

int AskForUninstallConfirmation() {
  int ret = ResultCodes::NORMAL_EXIT;
  views::Window::CreateChromeWindow(NULL, gfx::Rect(),
                                    new UninstallView(ret))->Show();
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
  return ret;
}

void ShowCloseBrowserFirstMessageBox() {
  const string16 text = l10n_util::GetStringUTF16(IDS_UNINSTALL_CLOSE_APP);
  const string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  const UINT flags = MB_OK | MB_ICONWARNING | MB_TOPMOST;
  ui::MessageBox(NULL, text, caption, flags);
}

int DoUninstallTasks(bool chrome_still_running) {
  // We want to show a warning to user (and exit) if Chrome is already running
  // *before* we show the uninstall confirmation dialog box. But while the
  // uninstall confirmation dialog is up, user might start Chrome, so we
  // check once again after user acknowledges Uninstall dialog.
  if (chrome_still_running) {
    ShowCloseBrowserFirstMessageBox();
    return ResultCodes::UNINSTALL_CHROME_ALIVE;
  }
  int ret = AskForUninstallConfirmation();
  if (Upgrade::IsBrowserAlreadyRunning()) {
    ShowCloseBrowserFirstMessageBox();
    return ResultCodes::UNINSTALL_CHROME_ALIVE;
  }

  if (ret != ResultCodes::UNINSTALL_USER_CANCEL) {
    // The following actions are just best effort.
    VLOG(1) << "Executing uninstall actions";
    if (!FirstRun::RemoveSentinel())
      VLOG(1) << "Failed to delete sentinel file.";
    // We want to remove user level shortcuts and we only care about the ones
    // created by us and not by the installer so |alternate| is false.
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    if (!ShellUtil::RemoveChromeDesktopShortcut(dist, ShellUtil::CURRENT_USER,
                                                false))
      VLOG(1) << "Failed to delete desktop shortcut.";
    if (!ShellUtil::RemoveChromeQuickLaunchShortcut(dist,
                                                    ShellUtil::CURRENT_USER))
      VLOG(1) << "Failed to delete quick launch shortcut.";
  }
  return ret;
}

// Prepares the localized strings that are going to be displayed to
// the user if the browser process dies. These strings are stored in the
// environment block so they are accessible in the early stages of the
// chrome executable's lifetime.
void PrepareRestartOnCrashEnviroment(const CommandLine& parsed_command_line) {
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
  string16 dlg_strings(l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_TITLE));
  dlg_strings.push_back('|');
  string16 adjusted_string(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_CONTENT));
  base::i18n::AdjustStringForLocaleDirection(&adjusted_string);
  dlg_strings.append(adjusted_string);
  dlg_strings.push_back('|');
  dlg_strings.append(ASCIIToUTF16(
      base::i18n::IsRTL() ? env_vars::kRtlLocale : env_vars::kLtrLocale));

  env->SetVar(env_vars::kRestartInfo, UTF16ToUTF8(dlg_strings));
}

bool RegisterApplicationRestart(const CommandLine& parsed_command_line) {
  DCHECK(base::win::GetVersion() >= base::win::VERSION_VISTA);
  base::ScopedNativeLibrary library(FilePath(L"kernel32.dll"));
  // Get the function pointer for RegisterApplicationRestart.
  RegisterApplicationRestartProc register_application_restart =
      static_cast<RegisterApplicationRestartProc>(
          library.GetFunctionPointer("RegisterApplicationRestart"));
  if (!register_application_restart)
    return false;

  // The Windows Restart Manager expects a string of command line flags only,
  // without the program.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitches(parsed_command_line);
  command_line.AppendArgs(parsed_command_line);
  // Ensure restore last session is set.
  if (!command_line.HasSwitch(switches::kRestoreLastSession))
    command_line.AppendSwitch(switches::kRestoreLastSession);

  // Restart Chrome if the computer is restarted as the result of an update.
  // This could be extended to handle crashes, hangs, and patches.
  HRESULT hr = register_application_restart(
      command_line.command_line_string().c_str(),
      RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH);
  DCHECK(SUCCEEDED(hr)) << "RegisterApplicationRestart failed.";
  return SUCCEEDED(hr);
}

// This method handles the --hide-icons and --show-icons command line options
// for chrome that get triggered by Windows from registry entries
// HideIconsCommand & ShowIconsCommand. Chrome doesn't support hide icons
// functionality so we just ask the users if they want to uninstall Chrome.
int HandleIconsCommands(const CommandLine& parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kHideIcons)) {
    string16 cp_applet;
    base::win::Version version = base::win::GetVersion();
    if (version >= base::win::VERSION_VISTA) {
      cp_applet.assign(L"Programs and Features");  // Windows Vista and later.
    } else if (version >= base::win::VERSION_XP) {
      cp_applet.assign(L"Add/Remove Programs");  // Windows XP.
    } else {
      return ResultCodes::UNSUPPORTED_PARAM;  // Not supported
    }

    const string16 msg =
        l10n_util::GetStringFUTF16(IDS_HIDE_ICONS_NOT_SUPPORTED, cp_applet);
    const string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    const UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST;
    if (IDOK == ui::MessageBox(NULL, msg, caption, flags))
      ShellExecute(NULL, NULL, L"appwiz.cpl", NULL, NULL, SW_SHOWNORMAL);
    return ResultCodes::NORMAL_EXIT;  // Exit as we are not launching browser.
  }
  // We don't hide icons so we shouldn't do anything special to show them
  return ResultCodes::UNSUPPORTED_PARAM;
}

// Check if there is any machine level Chrome installed on the current
// machine. If yes and the current Chrome process is user level, we do not
// allow the user level Chrome to run. So we notify the user and uninstall
// user level Chrome.
bool CheckMachineLevelInstall() {
  // TODO(tommi): Check if using the default distribution is always the right
  // thing to do.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  scoped_ptr<Version> version(InstallUtil::GetChromeVersion(dist, true));
  if (version.get()) {
    FilePath exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    std::wstring exe = exe_path.value();
    FilePath user_exe_path(installer::GetChromeInstallPath(false, dist));
    if (FilePath::CompareEqualIgnoreCase(exe, user_exe_path.value())) {
      const string16 text =
          l10n_util::GetStringUTF16(IDS_MACHINE_LEVEL_INSTALL_CONFLICT);
      const string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
      const UINT flags = MB_OK | MB_ICONERROR | MB_TOPMOST;
      ui::MessageBox(NULL, text, caption, flags);
      CommandLine uninstall_cmd(
          InstallUtil::GetChromeUninstallCmd(false, dist->GetType()));
      if (!uninstall_cmd.GetProgram().empty()) {
        uninstall_cmd.AppendSwitch(installer::switches::kForceUninstall);
        uninstall_cmd.AppendSwitch(
            installer::switches::kDoNotRemoveSharedItems);
        base::LaunchApp(uninstall_cmd, false, false, NULL);
      }
      return true;
    }
  }
  return false;
}

// BrowserMainPartsWin ---------------------------------------------------------

class BrowserMainPartsWin : public BrowserMainParts {
 public:
  explicit BrowserMainPartsWin(const MainFunctionParams& parameters)
      : BrowserMainParts(parameters) {}

 protected:
  virtual void PreEarlyInitialization() {
    // Initialize Winsock.
    net::EnsureWinsockInit();
  }

  virtual void PreMainMessageLoopStart() {
    OleInitialize(NULL);

    // If we're running tests (ui_task is non-null), then the ResourceBundle
    // has already been initialized.
    if (!parameters().ui_task) {
      // Override the configured locale with the user's preferred UI language.
      l10n_util::OverrideLocaleWithUILanguageList();
    }
  }

 private:
  virtual void InitializeSSL() {
    // Use NSS for SSL by default.
    // The default client socket factory uses NSS for SSL by default on
    // Windows.
    if (parsed_command_line().HasSwitch(switches::kUseSystemSSL)) {
      net::ClientSocketFactory::UseSystemSSL();
    } else {
      // We want to be sure to init NSPR on the main thread.
      base::EnsureNSPRInit();
    }
  }
};

// static
BrowserMainParts* BrowserMainParts::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return new BrowserMainPartsWin(parameters);
}
