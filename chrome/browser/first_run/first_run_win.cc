// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base/callback.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/process_util.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/metro.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run_import_observer.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/worker_thread_ticker.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "google_update/google_update_idl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/win/shell.h"

namespace {

// Launches the setup exe with the given parameter/value on the command-line.
// For non-metro Windows, it waits for its termination, returns its exit code
// in |*ret_code|, and returns true if the exit code is valid.
// For metro Windows, it launches setup via ShellExecuteEx and returns in order
// to bounce the user back to the desktop, then returns immediately.
bool LaunchSetupForEula(const base::FilePath::StringType& value,
                        int* ret_code) {
  base::FilePath exe_dir;
  if (!PathService::Get(base::DIR_MODULE, &exe_dir))
    return false;
  exe_dir = exe_dir.Append(installer::kInstallerDir);
  base::FilePath exe_path = exe_dir.Append(installer::kSetupExe);
  base::ProcessHandle ph;

  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitchNative(installer::switches::kShowEula, value);

  CommandLine* browser_command_line = CommandLine::ForCurrentProcess();
  if (browser_command_line->HasSwitch(switches::kChromeFrame)) {
    cl.AppendSwitch(switches::kChromeFrame);
  }

  if (base::win::IsMetroProcess()) {
    cl.AppendSwitch(installer::switches::kShowEulaForMetro);

    // This obscure use of the 'log usage' mask for windows 8 is documented here
    // http://go.microsoft.com/fwlink/?LinkID=243079. It causes the desktop
    // process to receive focus. Pass SEE_MASK_FLAG_NO_UI to avoid hangs if an
    // error occurs since the UI can't be shown from a metro process.
    ui::win::OpenAnyViaShell(exe_path.value(),
                             exe_dir.value(),
                             cl.GetCommandLineString(),
                             SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_FLAG_NO_UI);
    return false;
  } else {
    base::LaunchOptions launch_options;
    launch_options.wait = true;
    CommandLine setup_path(exe_path);
    setup_path.AppendArguments(cl, false);

    DWORD exit_code = 0;
    if (!base::LaunchProcess(setup_path, launch_options, &ph) ||
        !::GetExitCodeProcess(ph, &exit_code)) {
      return false;
    }

    *ret_code = exit_code;
    return true;
  }
}

// Populates |path| with the path to |file| in the sentinel directory. This is
// the application directory for user-level installs, and the default user data
// dir for system-level installs. Returns false on error.
bool GetSentinelFilePath(const wchar_t* file, base::FilePath* path) {
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;
  if (InstallUtil::IsPerUserInstall(exe_path.value().c_str()))
    *path = exe_path;
  else if (!PathService::Get(chrome::DIR_USER_DATA, path))
    return false;
  *path = path->Append(file);
  return true;
}

bool GetEULASentinelFilePath(base::FilePath* path) {
  return GetSentinelFilePath(installer::kEULASentinelFile, path);
}

// Returns true if the EULA is required but has not been accepted by this user.
// The EULA is considered having been accepted if the user has gotten past
// first run in the "other" environment (desktop or metro).
bool IsEULANotAccepted(installer::MasterPreferences* install_prefs) {
  bool value = false;
  if (install_prefs->GetBool(installer::master_preferences::kRequireEula,
          &value) && value) {
    base::FilePath eula_sentinel;
    // Be conservative and show the EULA if the path to the sentinel can't be
    // determined.
    if (!GetEULASentinelFilePath(&eula_sentinel) ||
        !file_util::PathExists(eula_sentinel)) {
      return true;
    }
  }
  return false;
}

// Writes the EULA to a temporary file, returned in |*eula_path|, and returns
// true if successful.
bool WriteEULAtoTempFile(base::FilePath* eula_path) {
  std::string terms = l10n_util::GetStringUTF8(IDS_TERMS_HTML);
  if (terms.empty())
    return false;
  FILE *file = file_util::CreateAndOpenTemporaryFile(eula_path);
  if (!file)
    return false;
  bool good = fwrite(terms.data(), terms.size(), 1, file) == 1;
  fclose(file);
  return good;
}

// Creates the sentinel indicating that the EULA was required and has been
// accepted.
bool CreateEULASentinel() {
  base::FilePath eula_sentinel;
  if (!GetEULASentinelFilePath(&eula_sentinel))
    return false;

  return (file_util::CreateDirectory(eula_sentinel.DirName()) &&
          file_util::WriteFile(eula_sentinel, "", 0) != -1);
}

// This class is used by first_run::ImportSettings to determine when the import
// process has ended and what was the result of the operation as reported by
// the process exit code. This class executes in the context of the main chrome
// process.
class ImportProcessRunner : public base::win::ObjectWatcher::Delegate {
 public:
  // The constructor takes the importer process to watch and then it does a
  // message loop blocking wait until the process ends. This object now owns
  // the import_process handle.
  explicit ImportProcessRunner(base::ProcessHandle import_process)
      : import_process_(import_process),
        exit_code_(content::RESULT_CODE_NORMAL_EXIT) {
    watcher_.StartWatching(import_process, this);
    MessageLoop::current()->Run();
  }
  virtual ~ImportProcessRunner() {
    ::CloseHandle(import_process_);
  }
  // Returns the child process exit code. There are 2 expected values:
  // NORMAL_EXIT, or IMPORTER_HUNG.
  int exit_code() const { return exit_code_; }

  // The child process has terminated. Find the exit code and quit the loop.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE {
    DCHECK(object == import_process_);
    if (!::GetExitCodeProcess(import_process_, &exit_code_)) {
      NOTREACHED();
    }
    MessageLoop::current()->Quit();
  }

 private:
  base::win::ObjectWatcher watcher_;
  base::ProcessHandle import_process_;
  DWORD exit_code_;
};

// Check every 3 seconds if the importer UI has hung.
const int kPollHangFrequency = 3000;

// This class specializes on finding hung 'owned' windows. Unfortunately, the
// HungWindowDetector class cannot be used here because it assumes child
// windows and not owned top-level windows.
// This code is executed in the context of the main browser process and will
// terminate the importer process if it is hung.
class HungImporterMonitor : public WorkerThreadTicker::Callback {
 public:
  // The ctor takes the owner popup window and the process handle of the
  // process to kill in case the popup or its owned active popup become
  // unresponsive.
  HungImporterMonitor(HWND owner_window, base::ProcessHandle import_process)
      : owner_window_(owner_window),
        import_process_(import_process),
        ticker_(kPollHangFrequency) {
    ticker_.RegisterTickHandler(this);
    ticker_.Start();
  }
  virtual ~HungImporterMonitor() {
    ticker_.Stop();
    ticker_.UnregisterTickHandler(this);
  }

 private:
  virtual void OnTick() OVERRIDE {
    if (!import_process_)
      return;
    // We find the top active popup that we own, this will be either the
    // owner_window_ itself or the dialog window of the other process. In
    // both cases it is worth hung testing because both windows share the
    // same message queue and at some point the other window could be gone
    // while the other process still not pumping messages.
    HWND active_window = ::GetLastActivePopup(owner_window_);
    if (::IsHungAppWindow(active_window) || ::IsHungAppWindow(owner_window_)) {
      ::TerminateProcess(import_process_, chrome::RESULT_CODE_IMPORTER_HUNG);
      import_process_ = NULL;
    }
  }

  HWND owner_window_;
  base::ProcessHandle import_process_;
  WorkerThreadTicker ticker_;
  DISALLOW_COPY_AND_ASSIGN(HungImporterMonitor);
};

std::string EncodeImportParams(int importer_type,
                               int options,
                               bool skip_first_run_ui) {
  return base::StringPrintf("%d@%d@%d", importer_type, options,
                            skip_first_run_ui ? 1 : 0);
}

bool DecodeImportParams(const std::string& encoded,
                        int* importer_type,
                        int* options,
                        bool* skip_first_run_ui) {
  std::vector<std::string> parts;
  base::SplitString(encoded, '@', &parts);
  int skip_first_run_ui_int;
  if ((parts.size() != 3) || !base::StringToInt(parts[0], importer_type) ||
      !base::StringToInt(parts[1], options) ||
      !base::StringToInt(parts[2], &skip_first_run_ui_int))
    return false;
  *skip_first_run_ui = !!skip_first_run_ui_int;
  return true;
}

#if !defined(USE_AURA)
// Imports browser items in this process. The browser and the items to
// import are encoded in the command line.
int ImportFromBrowser(Profile* profile,
                      const CommandLine& cmdline) {
  std::string import_info = cmdline.GetSwitchValueASCII(switches::kImport);
  if (import_info.empty()) {
    NOTREACHED();
    return false;
  }
  int importer_type = 0;
  int items_to_import = 0;
  bool skip_first_run_ui = false;
  if (!DecodeImportParams(import_info, &importer_type, &items_to_import,
                          &skip_first_run_ui)) {
    NOTREACHED();
    return false;
  }
  scoped_refptr<ImporterHost> importer_host(new ImporterHost);
  FirstRunImportObserver importer_observer;

  scoped_refptr<ImporterList> importer_list(new ImporterList(NULL));
  importer_list->DetectSourceProfilesHack();

  // If |skip_first_run_ui|, we run in headless mode.  This means that if
  // there is user action required the import is automatically canceled.
  if (skip_first_run_ui)
    importer_host->set_headless();

  importer::ShowImportProgressDialog(static_cast<uint16>(items_to_import),
      importer_host, &importer_observer,
      importer_list->GetSourceProfileForImporterType(importer_type), profile,
      true);
  importer_observer.RunLoop();
  return importer_observer.import_result();
}
#endif  // !defined(USE_AURA)

bool ImportSettingsWin(Profile* profile,
                       int importer_type,
                       int items_to_import,
                       const base::FilePath& import_bookmarks_path,
                       bool skip_first_run_ui) {
  if (!items_to_import && import_bookmarks_path.empty()) {
    return true;
  }

  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine import_cmd(cmdline.GetProgram());

  const char* kSwitchNames[] = {
    switches::kUserDataDir,
    switches::kChromeFrame,
    switches::kCountry,
  };
  import_cmd.CopySwitchesFrom(cmdline, kSwitchNames, arraysize(kSwitchNames));

  // Allow tests to introduce additional switches.
  import_cmd.AppendArguments(first_run::GetExtraArgumentsForImportProcess(),
                             false);

  // Since ImportSettings is called before the local state is stored on disk
  // we pass the language as an argument.  GetApplicationLocale checks the
  // current command line as fallback.
  import_cmd.AppendSwitchASCII(switches::kLang,
                               g_browser_process->GetApplicationLocale());

  if (items_to_import) {
    import_cmd.AppendSwitchASCII(switches::kImport,
        EncodeImportParams(importer_type, items_to_import, skip_first_run_ui));
  }

  if (!import_bookmarks_path.empty()) {
    import_cmd.AppendSwitchPath(switches::kImportFromFile,
                                import_bookmarks_path);
  }

  // The importer doesn't need to do any background networking tasks so disable
  // them.
  import_cmd.CommandLine::AppendSwitch(switches::kDisableBackgroundNetworking);

  // Time to launch the process that is going to do the import.
  base::ProcessHandle import_process;
  if (!base::LaunchProcess(import_cmd, base::LaunchOptions(), &import_process))
    return false;

  // We block inside the import_runner ctor, pumping messages until the
  // importer process ends. This can happen either by completing the import
  // or by hang_monitor killing it.
  ImportProcessRunner import_runner(import_process);

  // Import process finished. Reload the prefs, because importer may set
  // the pref value.
  if (profile)
    profile->GetPrefs()->ReloadPersistentPrefs();

  return (import_runner.exit_code() == content::RESULT_CODE_NORMAL_EXIT);
}

}  // namespace

namespace first_run {
namespace internal {

void DoPostImportPlatformSpecificTasks() {
  // Trigger the Active Setup command for system-level Chromes to finish
  // configuring this user's install (e.g. per-user shortcuts).
  // Delay the task slightly to give Chrome launch I/O priority while also
  // making sure shortcuts are created promptly to avoid annoying the user by
  // re-creating shortcuts he previously deleted.
  static const int64 kTiggerActiveSetupDelaySeconds = 5;
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
  } else if (!InstallUtil::IsPerUserInstall(chrome_exe.value().c_str())) {
    content::BrowserThread::GetBlockingPool()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&InstallUtil::TriggerActiveSetupCommand),
        base::TimeDelta::FromSeconds(kTiggerActiveSetupDelaySeconds));
  }
}

bool ImportSettings(Profile* profile,
                    scoped_refptr<ImporterHost> importer_host,
                    scoped_refptr<ImporterList> importer_list,
                    int items_to_import) {
  return ImportSettingsWin(
      profile,
      importer_list->GetSourceProfileAt(0).importer_type,
      items_to_import,
      base::FilePath(),
      false);
}

bool GetFirstRunSentinelFilePath(base::FilePath* path) {
  return GetSentinelFilePath(chrome::kFirstRunSentinel, path);
}

void SetImportPreferencesAndLaunchImport(
    MasterPrefs* out_prefs,
    installer::MasterPreferences* install_prefs) {
  std::string import_bookmarks_path;
  install_prefs->GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &import_bookmarks_path);

  if (!IsOrganicFirstRun()) {
    // If search engines aren't explicitly imported, don't import.
    if (!(out_prefs->do_import_items & importer::SEARCH_ENGINES)) {
      out_prefs->dont_import_items |= importer::SEARCH_ENGINES;
    }
    // If home page isn't explicitly imported, don't import.
    if (!(out_prefs->do_import_items & importer::HOME_PAGE)) {
      out_prefs->dont_import_items |= importer::HOME_PAGE;
    }
    // If history isn't explicitly forbidden, do import.
    if (!(out_prefs->dont_import_items & importer::HISTORY)) {
      out_prefs->do_import_items |= importer::HISTORY;
    }
  }

  if (out_prefs->do_import_items || !import_bookmarks_path.empty()) {
    // There is something to import from the default browser. This launches
    // the importer process and blocks until done or until it fails.
    scoped_refptr<ImporterList> importer_list(new ImporterList(NULL));
    importer_list->DetectSourceProfilesHack();
    if (!ImportSettingsWin(
        NULL, importer_list->GetSourceProfileAt(0).importer_type,
        out_prefs->do_import_items, base::FilePath::FromWStringHack(UTF8ToWide(
                                        import_bookmarks_path)), true)) {
      LOG(WARNING) << "silent import failed";
    }
  }
}

bool ShowPostInstallEULAIfNeeded(installer::MasterPreferences* install_prefs) {
  if (IsEULANotAccepted(install_prefs)) {
    // Show the post-installation EULA. This is done by setup.exe and the
    // result determines if we continue or not. We wait here until the user
    // dismisses the dialog.

    // The actual eula text is in a resource in chrome. We extract it to
    // a text file so setup.exe can use it as an inner frame.
    base::FilePath inner_html;
    if (WriteEULAtoTempFile(&inner_html)) {
      int retcode = 0;
      if (!LaunchSetupForEula(inner_html.value(), &retcode) ||
          (retcode != installer::EULA_ACCEPTED &&
           retcode != installer::EULA_ACCEPTED_OPT_IN)) {
        LOG(WARNING) << "EULA flow requires fast exit.";
        return false;
      }
      CreateEULASentinel();

      if (retcode == installer::EULA_ACCEPTED) {
        VLOG(1) << "EULA : no collection";
        GoogleUpdateSettings::SetCollectStatsConsent(false);
      } else if (retcode == installer::EULA_ACCEPTED_OPT_IN) {
        VLOG(1) << "EULA : collection consent";
        GoogleUpdateSettings::SetCollectStatsConsent(true);
      }
    }
  }
  return true;
}

}  // namespace internal
}  // namespace first_run

namespace first_run {

int ImportNow(Profile* profile, const CommandLine& cmdline) {
  int return_code = internal::ImportBookmarkFromFileIfNeeded(profile, cmdline);
#if !defined(USE_AURA)
  if (cmdline.HasSwitch(switches::kImport)) {
    return_code = ImportFromBrowser(profile, cmdline);
  }
#endif
  return return_code;
}

base::FilePath MasterPrefsPath() {
  // The standard location of the master prefs is next to the chrome binary.
  base::FilePath master_prefs;
  if (!PathService::Get(base::DIR_EXE, &master_prefs))
    return base::FilePath();
  return master_prefs.AppendASCII(installer::kDefaultMasterPrefs);
}

}  // namespace first_run
