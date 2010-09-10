// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/gtk/first_run_dialog.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"

namespace {

// This class acts as an observer for the ImporterHost::Observer::ImportEnded
// callback. When the import process is started, certain errors may cause
// ImportEnded() to be called synchronously, but the typical case is that
// ImportEnded() is called asynchronously. Thus we have to handle both cases.
class ImportEndedObserver : public ImporterHost::Observer {
 public:
   ImportEndedObserver()
       : ended_(false),
         quit_message_loop_(false) {
   }
   virtual ~ImportEndedObserver() {}

   virtual void ImportItemStarted(importer::ImportItem item) {}
   virtual void ImportItemEnded(importer::ImportItem item) {}
   virtual void ImportStarted() {}
   virtual void ImportEnded() {
     ended_ = true;
     if (quit_message_loop_)
       MessageLoop::current()->Quit();
   }

   void set_quit_message_loop() {
     quit_message_loop_ = true;
   }

   bool ended() {
     return ended_;
   }

 private:
  bool ended_;
  bool quit_message_loop_;
};

}  // namespace

// TODO(port): This is just a piece of the silent import functionality from
// ImportSettings for Windows.  It would be nice to get the rest of it ported.
bool FirstRun::ImportBookmarks(const FilePath& import_bookmarks_path) {
  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  CommandLine import_cmd(cmdline.GetProgram());

  // Propagate user data directory switch.
  if (cmdline.HasSwitch(switches::kUserDataDir)) {
    import_cmd.AppendSwitchPath(switches::kUserDataDir,
        cmdline.GetSwitchValuePath(switches::kUserDataDir));
  }
  // Since ImportSettings is called before the local state is stored on disk
  // we pass the language as an argument. GetApplicationLocale checks the
  // current command line as fallback.
  import_cmd.AppendSwitchASCII(switches::kLang,
                               g_browser_process->GetApplicationLocale());

  import_cmd.CommandLine::AppendSwitchPath(switches::kImportFromFile,
                                           import_bookmarks_path);
  // Time to launch the process that is going to do the import. We'll wait
  // for the process to return.
  return base::LaunchApp(import_cmd, true, false, NULL);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
CommandLine* Upgrade::new_command_line_ = NULL;
double Upgrade::saved_last_modified_time_of_exe_ = 0;

// static
bool Upgrade::IsUpdatePendingRestart() {
  return saved_last_modified_time_of_exe_ !=
      Upgrade::GetLastModifiedTimeOfExe();
}

// static
void Upgrade::SaveLastModifiedTimeOfExe() {
  saved_last_modified_time_of_exe_ = Upgrade::GetLastModifiedTimeOfExe();
}

// static
bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  return base::LaunchApp(command_line, false, false, NULL);
}

// static
double Upgrade::GetLastModifiedTimeOfExe() {
  FilePath exe_file_path;
  if (!PathService::Get(base::FILE_EXE, &exe_file_path)) {
    LOG(WARNING) << "Failed to get FilePath object for FILE_EXE.";
    return saved_last_modified_time_of_exe_;
  }
  base::PlatformFileInfo exe_file_info;
  if (!file_util::GetFileInfo(exe_file_path, &exe_file_info)) {
    LOG(WARNING) << "Failed to get FileInfo object for FILE_EXE - "
                 << exe_file_path.value();
    return saved_last_modified_time_of_exe_;
  }
  return exe_file_info.last_modified.ToDoubleT();
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

// At least for now, we do profile import in-process on Linux.
// static
bool FirstRun::ImportSettings(Profile* profile,
                              scoped_refptr<ImporterHost> importer_host,
                              int items_to_import) {
  // Import data.
  const ProfileInfo& source_profile = importer_host->GetSourceProfileInfoAt(0);
  scoped_ptr<ImportEndedObserver> observer(new ImportEndedObserver);
  importer_host->SetObserver(observer.get());
  importer_host->StartImportSettings(source_profile,
                                     profile,
                                     items_to_import,
                                     new ProfileWriter(profile),
                                     true);
  // If the import process has not errored out, block on it.
  if (!observer->ended()) {
    observer->set_quit_message_loop();
    MessageLoop::current()->Run();
  }
  // Unfortunately there's no success/fail signal in ImporterHost.
  return true;
}

// static
void FirstRun::ShowFirstRunDialog(Profile* profile,
                                  bool randomize_search_engine_experiment) {
  FirstRunDialog::Show(profile, randomize_search_engine_experiment);
}

// static
bool FirstRun::IsOrganic() {
  // We treat all installs as organic.
  return true;
}

// static
void FirstRun::PlatformSetup() {
  // Things that Windows does here (creating a desktop icon, for example) are
  // handled at install time on Linux.
}
