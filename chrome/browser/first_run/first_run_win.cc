// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <shlobj.h>
#include <windows.h>

#include <set>
#include <sstream>

#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/first_run/first_run_import_observer.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/worker_thread_ticker.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/common/notification_service.h"
#include "content/common/result_codes.h"
#include "google_update_idl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

namespace {

// Helper class that performs delayed first-run tasks that need more of the
// chrome infrastructure to be up and running before they can be attempted.
class FirstRunDelayedTasks : public NotificationObserver {
 public:
  enum Tasks {
    NO_TASK,
    INSTALL_EXTENSIONS
  };

  explicit FirstRunDelayedTasks(Tasks task) {
    if (task == INSTALL_EXTENSIONS) {
      registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                     NotificationService::AllSources());
    }
    registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                   NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // After processing the notification we always delete ourselves.
    if (type.value == NotificationType::EXTENSIONS_READY)
      DoExtensionWork(Source<Profile>(source).ptr()->GetExtensionService());
    delete this;
    return;
  }

 private:
  // Private ctor forces it to be created only in the heap.
  ~FirstRunDelayedTasks() {}

  // The extension work is to basically trigger an extension update check.
  // If the extension specified in the master pref is older than the live
  // extension it will get updated which is the same as get it installed.
  void DoExtensionWork(ExtensionService* service) {
    if (!service)
      return;
    service->updater()->CheckNow();
    return;
  }

  NotificationRegistrar registrar_;
};

// Creates the desktop shortcut to chrome for the current user. Returns
// false if it fails. It will overwrite the shortcut if it exists.
bool CreateChromeDesktopShortcut() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!dist)
    return false;
  return ShellUtil::CreateChromeDesktopShortcut(
      dist,
      chrome_exe.value(),
      dist->GetAppDescription(),
      ShellUtil::CURRENT_USER,
      false,
      true);  // create if doesn't exist.
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
      true);  // create if doesn't exist.
}

}  // namespace

bool FirstRun::LaunchSetupWithParam(const std::string& param,
                                    const std::wstring& value,
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

  if (!base::LaunchApp(cl, false, false, &ph))
    return false;
  DWORD wr = ::WaitForSingleObject(ph, INFINITE);
  if (wr != WAIT_OBJECT_0)
    return false;
  return (TRUE == ::GetExitCodeProcess(ph, reinterpret_cast<DWORD*>(ret_code)));
}

bool FirstRun::WriteEULAtoTempFile(FilePath* eula_path) {
  base::StringPiece terms =
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_TERMS_HTML);
  if (terms.empty())
    return false;
  FILE *file = file_util::CreateAndOpenTemporaryFile(eula_path);
  if (!file)
    return false;
  bool good = fwrite(terms.data(), terms.size(), 1, file) == 1;
  fclose(file);
  return good;
}

void FirstRun::DoDelayedInstallExtensions() {
  new FirstRunDelayedTasks(FirstRunDelayedTasks::INSTALL_EXTENSIONS);
}

namespace {

// This class is used by FirstRun::ImportSettings to determine when the import
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
        exit_code_(ResultCodes::NORMAL_EXIT) {
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
  virtual void OnObjectSignaled(HANDLE object) {
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
  virtual void OnTick() {
    if (!import_process_)
      return;
    // We find the top active popup that we own, this will be either the
    // owner_window_ itself or the dialog window of the other process. In
    // both cases it is worth hung testing because both windows share the
    // same message queue and at some point the other window could be gone
    // while the other process still not pumping messages.
    HWND active_window = ::GetLastActivePopup(owner_window_);
    if (::IsHungAppWindow(active_window) || ::IsHungAppWindow(owner_window_)) {
      ::TerminateProcess(import_process_, ResultCodes::IMPORTER_HUNG);
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
                               int skip_first_run_ui,
                               HWND window) {
  return base::StringPrintf(
      "%d@%d@%d@%d", importer_type, options, skip_first_run_ui, window);
}

bool DecodeImportParams(const std::string& encoded,
                        int* importer_type,
                        int* options,
                        int* skip_first_run_ui,
                        HWND* window) {
  std::vector<std::string> parts;
  base::SplitString(encoded, '@', &parts);
  if (parts.size() != 4)
    return false;

  if (!base::StringToInt(parts[0], importer_type))
    return false;

  if (!base::StringToInt(parts[1], options))
    return false;

  if (!base::StringToInt(parts[2], skip_first_run_ui))
    return false;

  int64 window_int;
  base::StringToInt64(parts[3], &window_int);
  *window = reinterpret_cast<HWND>(window_int);
  return true;
}

}  // namespace

// static
void FirstRun::PlatformSetup() {
  CreateChromeDesktopShortcut();
  // Windows 7 has deprecated the quick launch bar.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    CreateChromeQuickLaunchShortcut();
}

// static
bool FirstRun::IsOrganicFirstRun() {
  std::wstring brand;
  GoogleUpdateSettings::GetBrand(&brand);
  return GoogleUpdateSettings::IsOrganicFirstRun(brand);
}

// static
bool FirstRun::ImportSettings(Profile* profile,
                              int importer_type,
                              int items_to_import,
                              const FilePath& import_bookmarks_path,
                              bool skip_first_run_ui,
                              HWND parent_window) {
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
    import_cmd.CommandLine::AppendSwitchASCII(switches::kImport,
        EncodeImportParams(importer_type, items_to_import,
                           skip_first_run_ui ? 1 : 0, NULL));
  }

  if (!import_bookmarks_path.empty()) {
    import_cmd.CommandLine::AppendSwitchPath(
        switches::kImportFromFile, import_bookmarks_path);
  }

  // Time to launch the process that is going to do the import.
  base::ProcessHandle import_process;
  if (!base::LaunchApp(import_cmd, false, false, &import_process))
    return false;

  // We block inside the import_runner ctor, pumping messages until the
  // importer process ends. This can happen either by completing the import
  // or by hang_monitor killing it.
  ImportProcessRunner import_runner(import_process);

  // Import process finished. Reload the prefs, because importer may set
  // the pref value.
  if (profile)
    profile->GetPrefs()->ReloadPersistentPrefs();

  return (import_runner.exit_code() == ResultCodes::NORMAL_EXIT);
}

// static
bool FirstRun::ImportSettings(Profile* profile,
                              scoped_refptr<ImporterHost> importer_host,
                              scoped_refptr<ImporterList> importer_list,
                              int items_to_import) {
  return ImportSettings(
      profile,
      importer_list->GetSourceProfileAt(0).importer_type,
      items_to_import,
      FilePath(),
      false,
      NULL);
}

int FirstRun::ImportFromBrowser(Profile* profile,
                                const CommandLine& cmdline) {
  std::string import_info = cmdline.GetSwitchValueASCII(switches::kImport);
  if (import_info.empty()) {
    NOTREACHED();
    return false;
  }
  int importer_type = 0;
  int items_to_import = 0;
  int skip_first_run_ui = 0;
  HWND parent_window = NULL;
  if (!DecodeImportParams(import_info, &importer_type, &items_to_import,
                          &skip_first_run_ui, &parent_window)) {
    NOTREACHED();
    return false;
  }
  scoped_refptr<ImporterHost> importer_host(new ImporterHost);
  FirstRunImportObserver importer_observer;

  scoped_refptr<ImporterList> importer_list(new ImporterList);
  importer_list->DetectSourceProfilesHack();

  // If |skip_first_run_ui|, we run in headless mode.  This means that if
  // there is user action required the import is automatically canceled.
  if (skip_first_run_ui > 0)
    importer_host->set_headless();

  importer::ShowImportProgressDialog(
      parent_window,
      static_cast<uint16>(items_to_import),
      importer_host,
      &importer_observer,
      importer_list->GetSourceProfileForImporterType(importer_type),
      profile,
      true);
  importer_observer.RunLoop();
  return importer_observer.import_result();
}
