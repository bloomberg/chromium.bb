// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <shlobj.h>
#include <windows.h>

#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/first_run/first_run_import_observer.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_notification_types.h"
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

namespace {

// Helper class that performs delayed first-run tasks that need more of the
// chrome infrastructure to be up and running before they can be attempted.
class FirstRunDelayedTasks : public content::NotificationObserver {
 public:
  enum Tasks {
    NO_TASK,
    INSTALL_EXTENSIONS
  };

  explicit FirstRunDelayedTasks(Tasks task) {
    if (task == INSTALL_EXTENSIONS) {
      registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                     content::NotificationService::AllSources());
    }
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                   content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    // After processing the notification we always delete ourselves.
    if (type == chrome::NOTIFICATION_EXTENSIONS_READY) {
      DoExtensionWork(
          content::Source<Profile>(source).ptr()->GetExtensionService());
    }
    delete this;
  }

 private:
  // Private ctor forces it to be created only in the heap.
  ~FirstRunDelayedTasks() {}

  // The extension work is to basically trigger an extension update check.
  // If the extension specified in the master pref is older than the live
  // extension it will get updated which is the same as get it installed.
  void DoExtensionWork(ExtensionService* service) {
    if (service)
      service->updater()->CheckNow();
  }

  content::NotificationRegistrar registrar_;
};

// Creates the desktop shortcut to chrome for the current user. Returns
// false if it fails. It will overwrite the shortcut if it exists.
bool CreateChromeDesktopShortcut() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!dist || !dist->CanCreateDesktopShortcuts())
    return false;
  return ShellUtil::CreateChromeDesktopShortcut(
      dist,
      chrome_exe.value(),
      dist->GetAppDescription(),
      L"",
      L"",
      chrome_exe.value(),
      dist->GetIconIndex(),
      ShellUtil::CURRENT_USER,
      ShellUtil::SHORTCUT_CREATE_ALWAYS);
}

// Creates the quick launch shortcut to chrome for the current user. Returns
// false if it fails. It will overwrite the shortcut if it exists.
bool CreateChromeQuickLaunchShortcut() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return ShellUtil::CreateChromeQuickLaunchShortcut(
      dist,
      chrome_exe.value(),
      ShellUtil::CURRENT_USER,  // create only for current user.
      ShellUtil::SHORTCUT_CREATE_ALWAYS);
}

void PlatformSetup(Profile* profile) {
  CreateChromeDesktopShortcut();

  // Windows 7 has deprecated the quick launch bar.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    CreateChromeQuickLaunchShortcut();
}

// Launches the setup exe with the given parameter/value on the command-line,
// waits for its termination, returns its exit code in |*ret_code|, and
// returns true if the exit code is valid.
bool LaunchSetupWithParam(const std::string& param,
                          const FilePath::StringType& value,
                          int* ret_code) {
  FilePath exe_path;
  if (!PathService::Get(base::DIR_MODULE, &exe_path))
    return false;
  exe_path = exe_path.Append(installer::kInstallerDir);
  exe_path = exe_path.Append(installer::kSetupExe);
  base::ProcessHandle ph;
  CommandLine cl(exe_path);
  cl.AppendSwitchNative(param, value);

  CommandLine* browser_command_line = CommandLine::ForCurrentProcess();
  if (browser_command_line->HasSwitch(switches::kChromeFrame)) {
    cl.AppendSwitch(switches::kChromeFrame);
  }

  // TODO(evan): should this use options.wait = true?
  if (!base::LaunchProcess(cl, base::LaunchOptions(), &ph))
    return false;
  DWORD wr = ::WaitForSingleObject(ph, INFINITE);
  if (wr != WAIT_OBJECT_0)
    return false;
  return (TRUE == ::GetExitCodeProcess(ph, reinterpret_cast<DWORD*>(ret_code)));
}

// Returns true if the EULA is required but has not been accepted by this user.
// The EULA is considered having been accepted if the user has gotten past
// first run in the "other" environment (desktop or metro).
bool IsEulaNotAccepted(installer::MasterPreferences* install_prefs) {
  bool value = false;
  if (install_prefs->GetBool(installer::master_preferences::kRequireEula,
          &value) && value) {
    // Check for a first run sentinel in the alternate user data dir.
    FilePath alt_user_data_dir;
    if (!PathService::Get(chrome::DIR_ALT_USER_DATA, &alt_user_data_dir) ||
        !file_util::DirectoryExists(alt_user_data_dir) ||
        !file_util::PathExists(alt_user_data_dir.AppendASCII(
            first_run::internal::kSentinelFile))) {
      return true;
    }
  }
  return false;
}

// Writes the EULA to a temporary file, returned in |*eula_path|, and returns
// true if successful.
bool WriteEULAtoTempFile(FilePath* eula_path) {
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

void ShowPostInstallEULAIfNeeded(installer::MasterPreferences* install_prefs) {
  if (IsEulaNotAccepted(install_prefs)) {
    // Show the post-installation EULA. This is done by setup.exe and the
    // result determines if we continue or not. We wait here until the user
    // dismisses the dialog.

    // The actual eula text is in a resource in chrome. We extract it to
    // a text file so setup.exe can use it as an inner frame.
    FilePath inner_html;
    if (WriteEULAtoTempFile(&inner_html)) {
      int retcode = 0;
      if (!LaunchSetupWithParam(installer::switches::kShowEula,
                                inner_html.value(), &retcode) ||
          (retcode != installer::EULA_ACCEPTED &&
           retcode != installer::EULA_ACCEPTED_OPT_IN)) {
        LOG(WARNING) << "EULA rejected. Fast exit.";
        ::ExitProcess(1);
      }
      if (retcode == installer::EULA_ACCEPTED) {
        VLOG(1) << "EULA : no collection";
        GoogleUpdateSettings::SetCollectStatsConsent(false);
      } else if (retcode == installer::EULA_ACCEPTED_OPT_IN) {
        VLOG(1) << "EULA : collection consent";
        GoogleUpdateSettings::SetCollectStatsConsent(true);
      }
    }
  }
}

// Installs a task to do an extensions update check once the extensions system
// is running.
void DoDelayedInstallExtensions() {
  new FirstRunDelayedTasks(FirstRunDelayedTasks::INSTALL_EXTENSIONS);
}

void DoDelayedInstallExtensionsIfNeeded(
    installer::MasterPreferences* install_prefs) {
  DictionaryValue* extensions = 0;
  if (install_prefs->GetExtensionsBlock(&extensions)) {
    VLOG(1) << "Extensions block found in master preferences";
    DoDelayedInstallExtensions();
  }
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
                       const FilePath& import_bookmarks_path,
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

bool ImportSettings(Profile* profile,
                    scoped_refptr<ImporterHost> importer_host,
                    scoped_refptr<ImporterList> importer_list,
                    int items_to_import) {
  return ImportSettingsWin(
      profile,
      importer_list->GetSourceProfileAt(0).importer_type,
      items_to_import,
      FilePath(),
      false);
}

bool GetFirstRunSentinelFilePath(FilePath* path) {
  FilePath first_run_sentinel;

  FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;
  if (InstallUtil::IsPerUserInstall(exe_path.value().c_str())) {
    first_run_sentinel = exe_path;
  } else {
    if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
      return false;
  }

  *path = first_run_sentinel.AppendASCII(kSentinelFile);
  return true;
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
    if (!ImportSettingsWin(NULL,
          importer_list->GetSourceProfileAt(0).importer_type,
          out_prefs->do_import_items,
          FilePath::FromWStringHack(UTF8ToWide(import_bookmarks_path)), true)) {
      LOG(WARNING) << "silent import failed";
    }
  }
}

}  // namespace internal
}  // namespace first_run

namespace first_run {

void AutoImport(
    Profile* profile,
    bool homepage_defined,
    int import_items,
    int dont_import_items,
    bool make_chrome_default,
    ProcessSingleton* process_singleton) {
#if !defined(USE_AURA)
  // We need to avoid dispatching new tabs when we are importing because
  // that will lead to data corruption or a crash. Because there is no UI for
  // the import process, we pass NULL as the window to bring to the foreground
  // when a CopyData message comes in; this causes the message to be silently
  // discarded, which is the correct behavior during the import process.
  process_singleton->Lock(NULL);

  PlatformSetup(profile);

  scoped_refptr<ImporterHost> importer_host;
  importer_host = new ImporterHost;

  internal::AutoImportPlatformCommon(importer_host, profile, homepage_defined,
                                     import_items, dont_import_items,
                                     make_chrome_default);

  process_singleton->Unlock();
  CreateSentinel();
#endif  // !defined(USE_AURA)
}

int ImportNow(Profile* profile, const CommandLine& cmdline) {
  int return_code = internal::ImportBookmarkFromFileIfNeeded(profile, cmdline);
#if !defined(USE_AURA)
  if (cmdline.HasSwitch(switches::kImport)) {
    return_code = ImportFromBrowser(profile, cmdline);
  }
#endif
  return return_code;
}

FilePath MasterPrefsPath() {
  // The standard location of the master prefs is next to the chrome binary.
  FilePath master_prefs;
  if (!PathService::Get(base::DIR_EXE, &master_prefs))
    return FilePath();
  return master_prefs.AppendASCII(installer::kDefaultMasterPrefs);
}

bool ProcessMasterPreferences(const FilePath& user_data_dir,
                              MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  FilePath master_prefs_path;
  scoped_ptr<installer::MasterPreferences>
      install_prefs(internal::LoadMasterPrefs(&master_prefs_path));
  if (!install_prefs.get())
    return true;

  out_prefs->new_tabs = install_prefs->GetFirstRunTabs();

  internal::SetRLZPref(out_prefs, install_prefs.get());
  ShowPostInstallEULAIfNeeded(install_prefs.get());

  if (!internal::CopyPrefFile(user_data_dir, master_prefs_path))
    return true;

  DoDelayedInstallExtensionsIfNeeded(install_prefs.get());

  internal::SetupMasterPrefsFromInstallPrefs(out_prefs,
      install_prefs.get());

  // TODO(mirandac): Refactor skip-first-run-ui process into regular first run
  // import process.  http://crbug.com/49647
  // Note we are skipping all other master preferences if skip-first-run-ui
  // is *not* specified. (That is, we continue only if skipping first run ui.)
  if (!internal::SkipFirstRunUI(install_prefs.get()))
    return true;

  // We need to be able to create the first run sentinel or else we cannot
  // proceed because ImportSettings will launch the importer process which
  // would end up here if the sentinel is not present.
  if (!CreateSentinel())
    return false;

  internal::SetShowWelcomePagePrefIfNeeded(install_prefs.get());
  internal::SetImportPreferencesAndLaunchImport(out_prefs, install_prefs.get());
  internal::SetDefaultBrowser(install_prefs.get());

  return false;
}

}  // namespace first_run
