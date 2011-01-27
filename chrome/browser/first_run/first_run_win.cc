// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <set>
#include <sstream>

#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/views/first_run_search_engine_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/worker_thread_ticker.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update_idl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/image_view.h"
#include "views/controls/link.h"
#include "views/focus/accelerator_handler.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

namespace {

bool GetNewerChromeFile(FilePath* path) {
  if (!PathService::Get(base::DIR_EXE, path))
    return false;
  *path = path->Append(installer::kChromeNewExe);
  return true;
}

bool InvokeGoogleUpdateForRename() {
  ScopedComPtr<IProcessLauncher> ipl;
  if (!FAILED(ipl.CreateInstance(__uuidof(ProcessLauncherClass)))) {
    ULONG_PTR phandle = NULL;
    DWORD id = GetCurrentProcessId();
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    if (!FAILED(ipl->LaunchCmdElevated(dist->GetAppGuid().c_str(),
                                       google_update::kRegRenameCmdField,
                                       id, &phandle))) {
      HANDLE handle = HANDLE(phandle);
      WaitForSingleObject(handle, INFINITE);
      DWORD exit_code;
      ::GetExitCodeProcess(handle, &exit_code);
      ::CloseHandle(handle);
      if (exit_code == installer::RENAME_SUCCESSFUL)
        return true;
    }
  }
  return false;
}

bool LaunchSetupWithParam(const std::string& param, const std::wstring& value,
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

bool WriteEULAtoTempFile(FilePath* eula_path) {
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

CommandLine* Upgrade::new_command_line_ = NULL;

bool FirstRun::CreateChromeDesktopShortcut() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  if (!dist)
    return false;
  return ShellUtil::CreateChromeDesktopShortcut(dist, chrome_exe.value(),
      dist->GetAppDescription(), ShellUtil::CURRENT_USER,
      false, true);  // create if doesn't exist.
}

bool FirstRun::CreateChromeQuickLaunchShortcut() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return false;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return ShellUtil::CreateChromeQuickLaunchShortcut(dist, chrome_exe.value(),
      ShellUtil::CURRENT_USER,  // create only for current user.
      true);  // create if doesn't exist.
}

bool Upgrade::IsBrowserAlreadyRunning() {
  static HANDLE handle = NULL;
  FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  std::wstring exe = exe_path.value();
  std::replace(exe.begin(), exe.end(), '\\', '!');
  std::transform(exe.begin(), exe.end(), exe.begin(), tolower);
  exe = L"Global\\" + exe;
  if (handle != NULL)
    CloseHandle(handle);
  handle = CreateEvent(NULL, TRUE, TRUE, exe.c_str());
  int error = GetLastError();
  return (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED);
}

bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->UnSetVar(chrome::kChromeVersionEnvVar);
  return base::LaunchApp(command_line.command_line_string(),
                         false, false, NULL);
}

bool Upgrade::SwapNewChromeExeIfPresent() {
  FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  if (!file_util::PathExists(new_chrome_exe))
    return false;
  FilePath cur_chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &cur_chrome_exe))
    return false;

  // First try to rename exe by launching rename command ourselves.
  bool user_install =
      InstallUtil::IsPerUserInstall(cur_chrome_exe.value().c_str());
  HKEY reg_root = user_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  base::win::RegKey key;
  std::wstring rename_cmd;
  if ((key.Open(reg_root, dist->GetVersionKey().c_str(),
                KEY_READ) == ERROR_SUCCESS) &&
      (key.ReadValue(google_update::kRegRenameCmdField,
                     &rename_cmd) == ERROR_SUCCESS)) {
    base::ProcessHandle handle;
    if (base::LaunchApp(rename_cmd, true, true, &handle)) {
      DWORD exit_code;
      ::GetExitCodeProcess(handle, &exit_code);
      ::CloseHandle(handle);
      if (exit_code == installer::RENAME_SUCCESSFUL)
        return true;
    }
  }

  // Rename didn't work so try to rename by calling Google Update
  return InvokeGoogleUpdateForRename();
}

// static
bool Upgrade::DoUpgradeTasks(const CommandLine& command_line) {
  if (!Upgrade::SwapNewChromeExeIfPresent())
    return false;
  // At this point the chrome.exe has been swapped with the new one.
  if (!Upgrade::RelaunchChromeBrowser(command_line)) {
    // The re-launch fails. Feel free to panic now.
    NOTREACHED();
  }
  return true;
}

// static
bool Upgrade::IsUpdatePendingRestart() {
  FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  return file_util::PathExists(new_chrome_exe);
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
  // Returns the child process exit code. There are 3 expected values:
  // NORMAL_EXIT, IMPORTER_CANCEL or IMPORTER_HUNG.
  int exit_code() const {
    return exit_code_;
  }
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

std::string EncodeImportParams(int browser_type, int options,
                               int skip_first_run_ui, HWND window) {
  return StringPrintf("%d@%d@%d@%d", browser_type, options, skip_first_run_ui,
                      window);
}

bool DecodeImportParams(const std::string& encoded, int* browser_type,
                        int* options, int* skip_first_run_ui, HWND* window) {
  std::vector<std::string> parts;
  base::SplitString(encoded, '@', &parts);
  if (parts.size() != 4)
    return false;

  if (!base::StringToInt(parts[0], browser_type))
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
  FirstRun::CreateChromeDesktopShortcut();
  // Windows 7 has deprecated the quick launch bar.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    CreateChromeQuickLaunchShortcut();
}

// static
bool FirstRun::IsOrganic() {
  std::wstring brand;
  GoogleUpdateSettings::GetBrand(&brand);
  return GoogleUpdateSettings::IsOrganic(brand);
}

// static
void FirstRun::ShowFirstRunDialog(Profile* profile,
                                  bool randomize_search_engine_experiment) {
  // If the default search is managed via policy, we don't ask the user to
  // choose.
  TemplateURLModel* model = profile->GetTemplateURLModel();
  if (NULL == model || model->is_default_search_managed())
    return;

  views::Window* search_engine_dialog = views::Window::CreateChromeWindow(
      NULL,
      gfx::Rect(),
      new FirstRunSearchEngineView(profile,
      randomize_search_engine_experiment));
  DCHECK(search_engine_dialog);

  search_engine_dialog->Show();
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);
  search_engine_dialog->Close();
}

// static
bool FirstRun::ImportSettings(Profile* profile, int browser_type,
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
        EncodeImportParams(browser_type, items_to_import,
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
                              int items_to_import) {
  return ImportSettings(
      profile,
      importer_host->GetSourceProfileInfoAt(0).browser_type,
      items_to_import,
      FilePath(), false, NULL);
}

int FirstRun::ImportFromBrowser(Profile* profile,
                                const CommandLine& cmdline) {
  std::string import_info = cmdline.GetSwitchValueASCII(switches::kImport);
  if (import_info.empty()) {
    NOTREACHED();
    return false;
  }
  int browser_type = 0;
  int items_to_import = 0;
  int skip_first_run_ui = 0;
  HWND parent_window = NULL;
  if (!DecodeImportParams(import_info, &browser_type, &items_to_import,
                          &skip_first_run_ui, &parent_window)) {
    NOTREACHED();
    return false;
  }
  scoped_refptr<ImporterHost> importer_host = new ImporterHost();
  FirstRunImportObserver observer;

  // If |skip_first_run_ui|, we run in headless mode.  This means that if
  // there is user action required the import is automatically canceled.
  if (skip_first_run_ui > 0)
    importer_host->set_headless();

  StartImportingWithUI(
      parent_window,
      static_cast<uint16>(items_to_import),
      importer_host,
      importer_host->GetSourceProfileInfoForBrowserType(browser_type),
      profile,
      &observer,
      true);
  observer.RunLoop();
  return observer.import_result();
}

// static
bool FirstRun::InSearchExperimentLocale() {
  static std::set<std::string> allowed_locales;
  if (allowed_locales.empty()) {
    // List of locales in which search experiment can be run.
    allowed_locales.insert("en-GB");
    allowed_locales.insert("en-US");
  }
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::set<std::string>::iterator locale = allowed_locales.find(app_locale);
  return locale != allowed_locales.end();
}

//////////////////////////////////////////////////////////////////////////

namespace {

const wchar_t kHelpCenterUrl[] =
    L"http://www.google.com/support/chrome/bin/answer.py?answer=150752";

// This class displays a modal dialog using the views system. The dialog asks
// the user to give chrome another try. This class only handles the UI so the
// resulting actions are up to the caller. One version looks like this:
//
//   /----------------------------------------\
//   | |icon| You stopped using Google    [x] |
//   | |icon| Chrome. Would you like to..     |
//   |        [o] Give the new version a try  |
//   |        [ ] Uninstall Google Chrome     |
//   |        [ OK ] [Don't bug me]           |
//   |        _why_am_I_seeing this?__        |
//   ------------------------------------------
class TryChromeDialog : public views::ButtonListener,
                        public views::LinkController {
 public:
  explicit TryChromeDialog(size_t version)
      : version_(version),
        popup_(NULL),
        try_chrome_(NULL),
        kill_chrome_(NULL),
        result_(Upgrade::TD_LAST_ENUM) {
  }

  virtual ~TryChromeDialog() {
  };

  // Shows the modal dialog asking the user to try chrome. Note that the dialog
  // has no parent and it will position itself in a lower corner of the screen.
  // The dialog does not steal focus and does not have an entry in the taskbar.
  Upgrade::TryResult ShowModal() {
    using views::GridLayout;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    views::ImageView* icon = new views::ImageView();
    icon->SetImage(*rb.GetBitmapNamed(IDR_PRODUCT_ICON_32));
    gfx::Size icon_size = icon->GetPreferredSize();

    // An approximate window size. After Layout() we'll get better bounds.
    gfx::Rect pos(310, 160);
    views::WidgetWin* popup = new views::WidgetWin();
    if (!popup) {
      NOTREACHED();
      return Upgrade::TD_DIALOG_ERROR;
    }
    popup->set_delete_on_destroy(true);
    popup->set_window_style(WS_POPUP | WS_CLIPCHILDREN);
    popup->set_window_ex_style(WS_EX_TOOLWINDOW);
    popup->Init(NULL, pos);

    views::RootView* root_view = popup->GetRootView();
    // The window color is a tiny bit off-white.
    root_view->set_background(
        views::Background::CreateSolidBackground(0xfc, 0xfc, 0xfc));

    views::GridLayout* layout = views::GridLayout::CreatePanel(root_view);
    if (!layout) {
      NOTREACHED();
      return Upgrade::TD_DIALOG_ERROR;
    }
    root_view->SetLayoutManager(layout);

    views::ColumnSet* columns;
    // First row: [icon][pad][text][button].
    columns = layout->AddColumnSet(0);
    columns->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                       GridLayout::FIXED, icon_size.width(),
                       icon_size.height());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    columns->AddColumn(GridLayout::TRAILING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // Second row: [pad][pad][radio 1].
    columns = layout->AddColumnSet(1);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // Third row: [pad][pad][radio 2].
    columns = layout->AddColumnSet(2);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // Fourth row: [pad][pad][button][pad][button].
    columns = layout->AddColumnSet(3);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                       GridLayout::USE_PREF, 0, 0);
    columns->AddPaddingColumn(0, kRelatedButtonHSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                       GridLayout::USE_PREF, 0, 0);
    // Fifth row: [pad][pad][link].
    columns = layout->AddColumnSet(4);
    columns->AddPaddingColumn(0, icon_size.width());
    columns->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    columns->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                       GridLayout::USE_PREF, 0, 0);
    // First row views.
    layout->StartRow(0, 0);
    layout->AddView(icon);
    // The heading has two flavors of text, the alt one features extensions but
    // we only use it in the US until some international issues are fixed.
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    int heading_id;
    switch (version_) {
      case 0: heading_id = IDS_TRY_TOAST_HEADING; break;
      case 1: heading_id = IDS_TRY_TOAST_HEADING2; break;
      case 2: heading_id = IDS_TRY_TOAST_HEADING3; break;
      case 3: heading_id = IDS_TRY_TOAST_HEADING4; break;
      default:
        NOTREACHED() << "Cannot determine which headline to show.";
        return Upgrade::TD_DIALOG_ERROR;
    }
    string16 heading = l10n_util::GetStringUTF16(heading_id);
    views::Label* label = new views::Label(heading);
    label->SetFont(rb.GetFont(ResourceBundle::MediumBoldFont));
    label->SetMultiLine(true);
    label->SizeToFit(200);
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(label);
    // The close button is custom.
    views::ImageButton* close_button = new views::ImageButton(this);
    close_button->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
    close_button->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
    close_button->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
    close_button->set_tag(BT_CLOSE_BUTTON);
    layout->AddView(close_button);

    // Second row views.
    const string16 try_it(l10n_util::GetStringUTF16(IDS_TRY_TOAST_TRY_OPT));
    layout->StartRowWithPadding(0, 1, 0, 10);
    try_chrome_ = new views::RadioButton(try_it, 1);
    layout->AddView(try_chrome_);
    try_chrome_->SetChecked(true);

    // Third row views.
    const string16 kill_it(l10n_util::GetStringUTF16(IDS_UNINSTALL_CHROME));
    layout->StartRow(0, 2);
    kill_chrome_ = new views::RadioButton(kill_it, 1);
    layout->AddView(kill_chrome_);

    // Fourth row views.
    const string16 ok_it(l10n_util::GetStringUTF16(IDS_OK));
    const string16 cancel_it(l10n_util::GetStringUTF16(IDS_TRY_TOAST_CANCEL));
    const string16 why_this(l10n_util::GetStringUTF16(IDS_TRY_TOAST_WHY));
    layout->StartRowWithPadding(0, 3, 0, 10);
    views::Button* accept_button = new views::NativeButton(this, ok_it);
    accept_button->set_tag(BT_OK_BUTTON);
    layout->AddView(accept_button);
    views::Button* cancel_button = new views::NativeButton(this, cancel_it);
    cancel_button->set_tag(BT_CLOSE_BUTTON);
    layout->AddView(cancel_button);
    // Fifth row views.
    layout->StartRowWithPadding(0, 4, 0, 10);
    views::Link* link = new views::Link(why_this);
    link->SetController(this);
    layout->AddView(link);

    // We resize the window according to the layout manager. This takes into
    // account the differences between XP and Vista fonts and buttons.
    layout->Layout(root_view);
    gfx::Size preferred = layout->GetPreferredSize(root_view);
    pos = ComputeWindowPosition(preferred.width(), preferred.height(),
                                base::i18n::IsRTL());
    popup->SetBounds(pos);

    // Carve the toast shape into the window.
    SetToastRegion(popup->GetNativeView(),
                   preferred.width(), preferred.height());
    // Time to show the window in a modal loop.
    popup_ = popup;
    popup_->Show();
    MessageLoop::current()->Run();
    return result_;
  }

 protected:
  // Overridden from ButtonListener. We have two buttons and according to
  // what the user clicked we set |result_| and we should always close and
  // end the modal loop.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender->tag() == BT_CLOSE_BUTTON) {
      // The user pressed cancel or the [x] button.
      result_ = Upgrade::TD_NOT_NOW;
    } else if (!try_chrome_) {
      // We don't have radio buttons, the user pressed ok.
      result_ = Upgrade::TD_TRY_CHROME;
    } else {
      // The outcome is according to the selected ratio button.
      result_ = try_chrome_->checked() ? Upgrade::TD_TRY_CHROME :
                                         Upgrade::TD_UNINSTALL_CHROME;
    }
    popup_->Close();
    MessageLoop::current()->Quit();
  }

  // Overridden from LinkController. If the user selects the link we need to
  // fire off the default browser that by some convoluted logic should not be
  // chrome.
  virtual void LinkActivated(views::Link* source, int event_flags) {
    ::ShellExecuteW(NULL, L"open", kHelpCenterUrl, NULL, NULL, SW_SHOW);
  }

 private:
  enum ButtonTags {
    BT_NONE,
    BT_CLOSE_BUTTON,
    BT_OK_BUTTON,
  };

  // Returns a screen rectangle that is fit to show the window. In particular
  // it has the following properties: a) is visible and b) is attached to
  // the bottom of the working area. For LTR machines it returns a left side
  // rectangle and for RTL it returns a right side rectangle so that the
  // dialog does not compete with the standar place of the start menu.
  gfx::Rect ComputeWindowPosition(int width, int height, bool is_RTL) {
    // The 'Shell_TrayWnd' is the taskbar. We like to show our window in that
    // monitor if we can. This code works even if such window is not found.
    HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", NULL);
    HMONITOR monitor =
        ::MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info = {sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) {
      // Quite unexpected. Do a best guess at a visible rectangle.
      return gfx::Rect(20, 20, width + 20, height + 20);
    }
    // The |rcWork| is the work area. It should account for the taskbars that
    // are in the screen when we called the function.
    int left = is_RTL ? info.rcWork.left : info.rcWork.right - width;
    int top = info.rcWork.bottom - height;
    return gfx::Rect(left, top, width, height);
  }

  // Create a windows region that looks like a toast of width |w| and
  // height |h|. This is best effort, so we don't care much if the operation
  // fails.
  void SetToastRegion(HWND window, int w, int h) {
    static const POINT polygon[] = {
      {0,   4}, {1,   2}, {2,   1}, {4, 0},   // Left side.
      {w-4, 0}, {w-2, 1}, {w-1, 2}, {w, 4},   // Right side.
      {w, h}, {0, h}
    };
    HRGN region = ::CreatePolygonRgn(polygon, arraysize(polygon), WINDING);
    ::SetWindowRgn(window, region, FALSE);
  }

  // controls which version of the text to use.
  size_t version_;

  // We don't own any of this pointers. The |popup_| owns itself and owns
  // the other views.
  views::WidgetWin* popup_;
  views::RadioButton* try_chrome_;
  views::RadioButton* kill_chrome_;
  Upgrade::TryResult result_;

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialog);
};

}  // namespace

Upgrade::TryResult Upgrade::ShowTryChromeDialog(size_t version) {
  if (version > 10000) {
    // This is a test value. We want to make sure we exercise
    // returning this early. See EarlyReturnTest test harness.
    return Upgrade::TD_NOT_NOW;
  }
  TryChromeDialog td(version);
  return td.ShowModal();
}
