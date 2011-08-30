// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/first_run/first_run_import_observer.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "content/browser/user_metrics.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
// TODO(port): move more code in back from the first_run_win.cc module.
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#endif

namespace {

// The kSentinelFile file absence will tell us it is a first run.
const char kSentinelFile[] = "First Run";

FilePath GetDefaultPrefFilePath(bool create_profile_dir,
                                const FilePath& user_data_dir) {
  FilePath default_pref_dir =
      ProfileManager::GetDefaultProfileDir(user_data_dir);
  if (create_profile_dir) {
    if (!file_util::PathExists(default_pref_dir)) {
      if (!file_util::CreateDirectory(default_pref_dir))
        return FilePath();
    }
  }
  return ProfileManager::GetProfilePrefsPath(default_pref_dir);
}

// Sets the |items| bitfield according to whether the import data specified by
// |import_type| should be be auto imported or not.
void SetImportItem(PrefService* user_prefs,
                   const char* pref_path,
                   int import_items,
                   int dont_import_items,
                   importer::ImportItem import_type,
                   int& items) {
  // Work out whether an item is to be imported according to what is specified
  // in master preferences.
  bool should_import = false;
  bool master_pref_set =
      ((import_items | dont_import_items) & import_type) != 0;
  bool master_pref = ((import_items & ~dont_import_items) & import_type) != 0;

  if (import_type == importer::HISTORY ||
      ((import_type != importer::FAVORITES) &&
      FirstRun::IsOrganicFirstRun())) {
    // History is always imported unless turned off in master_preferences.
    // Search engines are only imported in certain builds unless overridden
    // in master_preferences.Home page is imported in organic builds only unless
    // turned off in master_preferences.
    should_import = !master_pref_set || master_pref;
  } else {
    // Bookmarks are never imported, unless turned on in master_preferences.
    // Search engine and home page import behaviour is similar in non organic
    // builds.
    should_import = master_pref_set && master_pref;
  }

  // If an import policy is set, import items according to policy. If no master
  // preference is set, but a corresponding recommended policy is set, import
  // item according to recommended policy. If both a master preference and a
  // recommended policy is set, the master preference wins. If neither
  // recommended nor managed policies are set, import item according to what we
  // worked out above.
  if (master_pref_set)
    user_prefs->SetBoolean(pref_path, should_import);

  if (!user_prefs->FindPreference(pref_path)->IsDefaultValue()) {
    if (user_prefs->GetBoolean(pref_path))
      items |= import_type;
  } else { // no policy (recommended or managed) is set
    if (should_import)
      items |= import_type;
  }

  user_prefs->ClearPref(pref_path);
}

}  // namespace

// FirstRun -------------------------------------------------------------------

FirstRun::FirstRunState FirstRun::first_run_ = FIRST_RUN_UNKNOWN;

FirstRun::MasterPrefs::MasterPrefs()
    : ping_delay(0),
      homepage_defined(false),
      do_import_items(0),
      dont_import_items(0),
      run_search_engine_experiment(false),
      randomize_search_engine_experiment(false),
      make_chrome_default(false) {
}

FirstRun::MasterPrefs::~MasterPrefs() {}

// TODO(port): Import switches need to be ported to both Mac and Linux. Not all
// import switches here are implemented for Linux. None are implemented for Mac
// (as this function will not be called on Mac).
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  int return_code = true;
  if (cmdline.HasSwitch(switches::kImportFromFile)) {
    // Silently import preset bookmarks from file.
    // This is an OEM scenario.
    return_code = ImportFromFile(profile, cmdline);
  }
  if (cmdline.HasSwitch(switches::kImport)) {
#if defined(OS_WIN)
    return_code = ImportFromBrowser(profile, cmdline);
#else
    NOTIMPLEMENTED();
#endif
  }
  return return_code;
}

// static
bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  FilePath master_prefs = MasterPrefsPath();
  if (master_prefs.empty())
    return true;
  installer::MasterPreferences prefs(master_prefs);
  if (!prefs.read_from_file())
    return true;

  out_prefs->new_tabs = prefs.GetFirstRunTabs();

  bool value = false;

#if defined(OS_WIN)
  // RLZ is currently a Windows-only phenomenon.  When it comes to the Mac/
  // Linux, enable it here.
  if (!prefs.GetInt(installer::master_preferences::kDistroPingDelay,
                    &out_prefs->ping_delay)) {
    // 90 seconds is the default that we want to use in case master
    // preferences is missing, corrupt or ping_delay is missing.
    out_prefs->ping_delay = 90;
  }

  if (prefs.GetBool(installer::master_preferences::kRequireEula, &value) &&
      value) {
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
#endif

  if (prefs.GetBool(installer::master_preferences::kAltFirstRunBubble,
                    &value) && value) {
    FirstRun::SetOEMFirstRunBubblePref();
  }

  FilePath user_prefs = GetDefaultPrefFilePath(true, user_data_dir);
  if (user_prefs.empty())
    return true;

  // The master prefs are regular prefs so we can just copy the file
  // to the default place and they just work.
  if (!file_util::CopyFile(master_prefs, user_prefs))
    return true;

#if defined(OS_WIN)
  DictionaryValue* extensions = 0;
  if (prefs.GetExtensionsBlock(&extensions)) {
    VLOG(1) << "Extensions block found in master preferences";
    DoDelayedInstallExtensions();
  }
#endif

  if (prefs.GetBool(installer::master_preferences::kDistroImportSearchPref,
                    &value)) {
    if (value) {
      out_prefs->do_import_items |= importer::SEARCH_ENGINES;
    } else {
      out_prefs->dont_import_items |= importer::SEARCH_ENGINES;
    }
  }

  // Check to see if search engine logos should be randomized.
  if (prefs.GetBool(
          installer::master_preferences::
              kSearchEngineExperimentRandomizePref,
          &value) && value) {
    out_prefs->randomize_search_engine_experiment = true;
  }

  // If we're suppressing the first-run bubble, set that preference now.
  // Otherwise, wait until the user has completed first run to set it, so the
  // user is guaranteed to see the bubble iff he or she has completed the first
  // run process.
  if (prefs.GetBool(
          installer::master_preferences::kDistroSuppressFirstRunBubble,
          &value) && value)
    FirstRun::SetShowFirstRunBubblePref(false);

  if (prefs.GetBool(
          installer::master_preferences::kDistroImportHistoryPref,
          &value)) {
    if (value) {
      out_prefs->do_import_items |= importer::HISTORY;
    } else {
      out_prefs->dont_import_items |= importer::HISTORY;
    }
  }

  std::string not_used;
  out_prefs->homepage_defined = prefs.GetString(prefs::kHomePage, &not_used);

  if (prefs.GetBool(
          installer::master_preferences::kDistroImportHomePagePref,
          &value)) {
    if (value) {
      out_prefs->do_import_items |= importer::HOME_PAGE;
    } else {
      out_prefs->dont_import_items |= importer::HOME_PAGE;
    }
  }

  // Bookmarks are never imported unless specifically turned on.
  if (prefs.GetBool(
          installer::master_preferences::kDistroImportBookmarksPref,
          &value)) {
    if (value)
      out_prefs->do_import_items |= importer::FAVORITES;
    else
      out_prefs->dont_import_items |= importer::FAVORITES;
  }

  if (prefs.GetBool(
          installer::master_preferences::kMakeChromeDefaultForUser,
          &value) && value) {
    out_prefs->make_chrome_default = true;
  }

  // TODO(mirandac): Refactor skip-first-run-ui process into regular first run
  // import process.  http://crbug.com/49647
  // Note we are skipping all other master preferences if skip-first-run-ui
  // is *not* specified. (That is, we continue only if skipping first run ui.)
  if (!prefs.GetBool(
          installer::master_preferences::kDistroSkipFirstRunPref,
          &value) || !value) {
    return true;
  }

#if !defined(OS_WIN)
  // From here on we won't show first run so we need to do the work to show the
  // bubble anyway, unless it's already been explicitly suppressed.
  FirstRun::SetShowFirstRunBubblePref(true);
#endif

  // We need to be able to create the first run sentinel or else we cannot
  // proceed because ImportSettings will launch the importer process which
  // would end up here if the sentinel is not present.
  if (!FirstRun::CreateSentinel())
    return false;

  if (prefs.GetBool(installer::master_preferences::kDistroShowWelcomePage,
                    &value) && value) {
    FirstRun::SetShowWelcomePagePref();
  }

  std::string import_bookmarks_path;
  prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &import_bookmarks_path);

#if defined(OS_WIN)
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
    scoped_refptr<ImporterList> importer_list(new ImporterList);
    importer_list->DetectSourceProfilesHack();
    if (!FirstRun::ImportSettings(NULL,
          importer_list->GetSourceProfileAt(0).importer_type,
          out_prefs->do_import_items,
          FilePath::FromWStringHack(UTF8ToWide(import_bookmarks_path)),
          true, NULL)) {
      LOG(WARNING) << "silent import failed";
    }
  }
#else
  if (!import_bookmarks_path.empty()) {
    // There are bookmarks to import from a file.
    FilePath path = FilePath::FromWStringHack(UTF8ToWide(
        import_bookmarks_path));
    if (!FirstRun::ImportBookmarks(path)) {
      LOG(WARNING) << "silent bookmark import failed";
    }
  }
#endif

  // Even on the first run we only allow for the user choice to take effect if
  // no policy has been set by the admin.
  if (!g_browser_process->local_state()->IsManagedPreference(
          prefs::kDefaultBrowserSettingEnabled)) {
    if (prefs.GetBool(
            installer::master_preferences::kMakeChromeDefaultForUser,
            &value) && value) {
      ShellIntegration::SetAsDefaultBrowser();
    }
  } else {
    if (g_browser_process->local_state()->GetBoolean(
            prefs::kDefaultBrowserSettingEnabled)) {
      ShellIntegration::SetAsDefaultBrowser();
    }
  }

  return false;
}

// static
bool FirstRun::IsChromeFirstRun() {
  if (first_run_ != FIRST_RUN_UNKNOWN)
    return first_run_ == FIRST_RUN_TRUE;

  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel) ||
      file_util::PathExists(first_run_sentinel)) {
    first_run_ = FIRST_RUN_FALSE;
    return false;
  }
  first_run_ = FIRST_RUN_TRUE;
  return true;
}

// static
bool FirstRun::RemoveSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::Delete(first_run_sentinel, false);
}

// static
bool FirstRun::CreateSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::WriteFile(first_run_sentinel, "", 0) != -1;
}

// static
bool FirstRun::SetShowFirstRunBubblePref(bool show_bubble) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldShowFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
    local_state->SetBoolean(prefs::kShouldShowFirstRunBubble, show_bubble);
  }
  return true;
}

// static
bool FirstRun::SetShowWelcomePagePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldShowWelcomePage)) {
    local_state->RegisterBooleanPref(prefs::kShouldShowWelcomePage, false);
    local_state->SetBoolean(prefs::kShouldShowWelcomePage, true);
  }
  return true;
}

// static
bool FirstRun::SetPersonalDataManagerFirstRunPref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(
          prefs::kAutofillPersonalDataManagerFirstRun)) {
    local_state->RegisterBooleanPref(
        prefs::kAutofillPersonalDataManagerFirstRun, false);
    local_state->SetBoolean(prefs::kAutofillPersonalDataManagerFirstRun, true);
  }
  return true;
}

// static
bool FirstRun::SearchEngineSelectorDisallowed() {
  // For now, the only case in which the search engine dialog should never be
  // shown is if the locale is Russia.
  std::string locale = g_browser_process->GetApplicationLocale();
  return (locale == "ru");
}

// static
bool FirstRun::SetOEMFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldUseOEMFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldUseOEMFirstRunBubble, false);
    local_state->SetBoolean(prefs::kShouldUseOEMFirstRunBubble, true);
  }
  return true;
}

// static
bool FirstRun::SetMinimalFirstRunBubblePref() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (!local_state->FindPreference(prefs::kShouldUseMinimalFirstRunBubble)) {
    local_state->RegisterBooleanPref(prefs::kShouldUseMinimalFirstRunBubble,
                                     false);
    local_state->SetBoolean(prefs::kShouldUseMinimalFirstRunBubble, true);
  }
  return true;
}

// static
int FirstRun::ImportFromFile(Profile* profile, const CommandLine& cmdline) {
  FilePath file_path = cmdline.GetSwitchValuePath(switches::kImportFromFile);
  if (file_path.empty()) {
    NOTREACHED();
    return false;
  }
  scoped_refptr<ImporterHost> importer_host(new ImporterHost);
  importer_host->set_headless();

  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  source_profile.source_path = file_path;

  FirstRunImportObserver importer_observer;
  importer::ShowImportProgressDialog(NULL,
                                     importer::FAVORITES,
                                     importer_host,
                                     &importer_observer,
                                     source_profile,
                                     profile,
                                     true);

  importer_observer.RunLoop();
  return importer_observer.import_result();
}

// static
bool FirstRun::GetFirstRunSentinelFilePath(FilePath* path) {
  FilePath first_run_sentinel;

#if defined(OS_WIN)
  FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;
  if (InstallUtil::IsPerUserInstall(exe_path.value().c_str())) {
    first_run_sentinel = exe_path;
  } else {
    if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
      return false;
  }
#else
  if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
    return false;
#endif

  *path = first_run_sentinel.AppendASCII(kSentinelFile);
  return true;
}

// static
void FirstRun::AutoImport(
    Profile* profile,
    bool homepage_defined,
    int import_items,
    int dont_import_items,
    bool search_engine_experiment,
    bool randomize_search_engine_experiment,
    bool make_chrome_default,
    ProcessSingleton* process_singleton) {
  // We need to avoid dispatching new tabs when we are importing because
  // that will lead to data corruption or a crash. Because there is no UI for
  // the import process, we pass NULL as the window to bring to the foreground
  // when a CopyData message comes in; this causes the message to be silently
  // discarded, which is the correct behavior during the import process.
  process_singleton->Lock(NULL);

  PlatformSetup();

  FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = file_util::PathExists(local_state_path);

  scoped_refptr<ImporterHost> importer_host;
  // TODO(csilv,mirandac): Out-of-process import has only been qualified on
  // MacOS X, so we will only use it on that platform since it is required.
  // Remove this conditional logic once oop import is qualified for
  // Linux/Windows. http://crbug.com/22142
#if defined(OS_MACOSX)
  importer_host = new ExternalProcessImporterHost;
#else
  importer_host = new ImporterHost;
#endif

  scoped_refptr<ImporterList> importer_list(new ImporterList);
  importer_list->DetectSourceProfilesHack();

  // Do import if there is an available profile for us to import.
  if (importer_list->count() > 0) {
    // Don't show the warning dialog if import fails.
    importer_host->set_headless();
    int items = 0;

    if (IsOrganicFirstRun()) {
      // Home page is imported in organic builds only unless turned off or
      // defined in master_preferences.
      if (homepage_defined) {
        dont_import_items |= importer::HOME_PAGE;
        if (import_items & importer::HOME_PAGE)
          import_items &= ~importer::HOME_PAGE;
      }
      // Search engines are not imported automatically in organic builds if the
      // user already has a user preferences directory.
      if (local_state_file_exists) {
        dont_import_items |= importer::SEARCH_ENGINES;
        if (import_items & importer::SEARCH_ENGINES)
          import_items &= ~importer::SEARCH_ENGINES;
      }
    }

    PrefService* user_prefs = profile->GetPrefs();

    SetImportItem(user_prefs,
                  prefs::kImportHistory,
                  import_items,
                  dont_import_items,
                  importer::HISTORY,
                  items);
    SetImportItem(user_prefs,
                  prefs::kImportHomepage,
                  import_items,
                  dont_import_items,
                  importer::HOME_PAGE,
                  items);
    SetImportItem(user_prefs,
                  prefs::kImportSearchEngine,
                  import_items,
                  dont_import_items,
                  importer::SEARCH_ENGINES,
                  items);
    SetImportItem(user_prefs,
                  prefs::kImportBookmarks,
                  import_items,
                  dont_import_items,
                  importer::FAVORITES,
                  items);

    ImportSettings(profile, importer_host, importer_list, items);
  }

  UserMetrics::RecordAction(UserMetricsAction("FirstRunDef_Accept"));

  // Launch the search engine dialog only for certain builds, and only if the
  // user has not already set preferences.
  if (IsOrganicFirstRun() && !local_state_file_exists) {
    // The home page string may be set in the preferences, but the user should
    // initially use Chrome with the NTP as home page in organic builds.
    profile->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage, true);
    first_run::ShowFirstRunDialog(profile, randomize_search_engine_experiment);
  }

  if (make_chrome_default)
    ShellIntegration::SetAsDefaultBrowser();

  // Don't display the minimal bubble if there is no default search provider.
  TemplateURLService* search_engines_model =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (search_engines_model &&
      search_engines_model->GetDefaultSearchProvider()) {
    FirstRun::SetShowFirstRunBubblePref(true);
    // Set the first run bubble to minimal.
    FirstRun::SetMinimalFirstRunBubblePref();
  }
  FirstRun::SetShowWelcomePagePref();
  FirstRun::SetPersonalDataManagerFirstRunPref();

  process_singleton->Unlock();
  FirstRun::CreateSentinel();
}

#if defined(OS_POSIX)
namespace {

// This class acts as an observer for the ImporterProgressObserver::ImportEnded
// callback. When the import process is started, certain errors may cause
// ImportEnded() to be called synchronously, but the typical case is that
// ImportEnded() is called asynchronously. Thus we have to handle both cases.
class ImportEndedObserver : public importer::ImporterProgressObserver {
 public:
  ImportEndedObserver() : ended_(false),
                          should_quit_message_loop_(false) {}
  virtual ~ImportEndedObserver() {}

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE {}
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE {}
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE {}
  virtual void ImportEnded() OVERRIDE {
    ended_ = true;
    if (should_quit_message_loop_)
      MessageLoop::current()->Quit();
  }

  void set_should_quit_message_loop() {
    should_quit_message_loop_ = true;
  }

  bool ended() {
    return ended_;
  }

 private:
  // Set if the import has ended.
  bool ended_;

  // Set by the client (via set_should_quit_message_loop) if, when the import
  // ends, this class should quit the message loop.
  bool should_quit_message_loop_;
};

}  // namespace

// static
bool FirstRun::ImportSettings(Profile* profile,
                              scoped_refptr<ImporterHost> importer_host,
                              scoped_refptr<ImporterList> importer_list,
                              int items_to_import) {
  const importer::SourceProfile& source_profile =
      importer_list->GetSourceProfileAt(0);

  // Ensure that importers aren't requested to import items that they do not
  // support.
  items_to_import &= source_profile.services_supported;

  scoped_ptr<ImportEndedObserver> observer(new ImportEndedObserver);
  importer_host->SetObserver(observer.get());
  importer_host->StartImportSettings(source_profile,
                                     profile,
                                     items_to_import,
                                     new ProfileWriter(profile),
                                     true);
  // If the import process has not errored out, block on it.
  if (!observer->ended()) {
    observer->set_should_quit_message_loop();
    MessageLoop::current()->Run();
  }

  // Unfortunately there's no success/fail signal in ImporterHost.
  return true;
}

#endif  // OS_POSIX
