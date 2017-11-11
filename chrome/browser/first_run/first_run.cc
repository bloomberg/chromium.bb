// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_uma.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

using base::UserMetricsAction;

namespace {

// A bitfield formed from values in AutoImportState to record the state of
// AutoImport. This is used in testing to verify import startup actions that
// occur before an observer can be registered in the test.
uint16_t g_auto_import_state = first_run::AUTO_IMPORT_NONE;

// Flags for functions of similar name.
bool g_should_show_welcome_page = false;
bool g_should_do_autofill_personal_data_manager_first_run = false;

// Indicates whether this is first run. Populated when IsChromeFirstRun
// is invoked, then used as a cache on subsequent calls.
first_run::internal::FirstRunState g_first_run =
    first_run::internal::FIRST_RUN_UNKNOWN;

// This class acts as an observer for the ImporterProgressObserver::ImportEnded
// callback. When the import process is started, certain errors may cause
// ImportEnded() to be called synchronously, but the typical case is that
// ImportEnded() is called asynchronously. Thus we have to handle both cases.
class ImportEndedObserver : public importer::ImporterProgressObserver {
 public:
  ImportEndedObserver() : ended_(false) {}
  ~ImportEndedObserver() override {}

  // importer::ImporterProgressObserver:
  void ImportStarted() override {}
  void ImportItemStarted(importer::ImportItem item) override {}
  void ImportItemEnded(importer::ImportItem item) override {}
  void ImportEnded() override {
    ended_ = true;
    if (!callback_for_import_end_.is_null())
      callback_for_import_end_.Run();
  }

  void set_callback_for_import_end(const base::Closure& callback) {
    callback_for_import_end_ = callback;
  }

  bool ended() const {
    return ended_;
  }

 private:
  // Set if the import has ended.
  bool ended_;

  base::Closure callback_for_import_end_;

  DISALLOW_COPY_AND_ASSIGN(ImportEndedObserver);
};

// Helper class that performs delayed first-run tasks that need more of the
// chrome infrastructure to be up and running before they can be attempted.
class FirstRunDelayedTasks : public content::NotificationObserver {
 public:
  FirstRunDelayedTasks() : weak_ptr_factory_(this) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_PROFILE_CREATED: {
        content::BrowserContext* context =
            content::Source<Profile>(source).ptr();
        extensions::ExtensionSystem::Get(context)->ready().Post(
            FROM_HERE, base::Bind(&FirstRunDelayedTasks::OnExtensionSystemReady,
                                  weak_ptr_factory_.GetWeakPtr(), context));
        break;
      }
      case chrome::NOTIFICATION_BROWSER_CLOSED: {
        delete this;
        break;
      }
      default:
        NOTREACHED();
    }
  }

 private:
  // Private ctor forces it to be created only in the heap.
  ~FirstRunDelayedTasks() override {}

  void OnExtensionSystemReady(content::BrowserContext* context) {
    // Process the notification and delete this.
    ExtensionService* service =
        extensions::ExtensionSystem::Get(context)->extension_service();
    if (service) {
      // Trigger an extension update check. If the extension specified in the
      // master pref is older than the live extension it will get updated which
      // is the same as get it installed.
      service->updater()->CheckNow(extensions::ExtensionUpdater::CheckParams());
    }
    delete this;
  }

  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<FirstRunDelayedTasks> weak_ptr_factory_;
};

// Installs a task to do an extensions update check once the extensions system
// is running.
void DoDelayedInstallExtensions() {
  new FirstRunDelayedTasks();
}

void DoDelayedInstallExtensionsIfNeeded(
    installer::MasterPreferences* install_prefs) {
  base::DictionaryValue* extensions = 0;
  if (install_prefs->GetExtensionsBlock(&extensions)) {
    DVLOG(1) << "Extensions block found in master preferences";
    DoDelayedInstallExtensions();
  }
}

// Launches the import, via |importer_host|, from |source_profile| into
// |target_profile| for the items specified in the |items_to_import| bitfield.
// This may be done in a separate process depending on the platform, but it will
// always block until done.
void ImportFromSourceProfile(const importer::SourceProfile& source_profile,
                             Profile* target_profile,
                             uint16_t items_to_import) {
  // Deletes itself.
  ExternalProcessImporterHost* importer_host =
      new ExternalProcessImporterHost;
  // Don't show the warning dialog if import fails.
  importer_host->set_headless();

  ImportEndedObserver observer;
  importer_host->set_observer(&observer);
  importer_host->StartImportSettings(source_profile,
                                     target_profile,
                                     items_to_import,
                                     new ProfileWriter(target_profile));
  // If the import process has not errored out, block on it.
  if (!observer.ended()) {
    base::RunLoop loop;
    observer.set_callback_for_import_end(loop.QuitClosure());
    loop.Run();
    observer.set_callback_for_import_end(base::Closure());
  }
}

// Imports bookmarks from an html file whose path is provided by
// |import_bookmarks_path|.
void ImportFromFile(Profile* profile,
                    const std::string& import_bookmarks_path) {
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;

  const base::FilePath::StringType& import_bookmarks_path_str =
#if defined(OS_WIN)
      base::UTF8ToUTF16(import_bookmarks_path);
#else
      import_bookmarks_path;
#endif
  source_profile.source_path = base::FilePath(import_bookmarks_path_str);

  ImportFromSourceProfile(source_profile, profile, importer::FAVORITES);
  g_auto_import_state |= first_run::AUTO_IMPORT_BOOKMARKS_FILE_IMPORTED;
}

// Imports settings from the first profile in |importer_list|.
void ImportSettings(Profile* profile,
                    std::unique_ptr<ImporterList> importer_list,
                    uint16_t items_to_import) {
  DCHECK(items_to_import);
  const importer::SourceProfile& source_profile =
      importer_list->GetSourceProfileAt(0);

  // Ensure that importers aren't requested to import items that they do not
  // support. If there is no overlap, skip.
  items_to_import &= source_profile.services_supported;
  if (items_to_import)
    ImportFromSourceProfile(source_profile, profile, items_to_import);

  g_auto_import_state |= first_run::AUTO_IMPORT_PROFILE_IMPORTED;
}

GURL UrlFromString(const std::string& in) {
  return GURL(in);
}

void ConvertStringVectorToGURLVector(
    const std::vector<std::string>& src,
    std::vector<GURL>* ret) {
  ret->resize(src.size());
  std::transform(src.begin(), src.end(), ret->begin(), &UrlFromString);
}

// Show the first run search engine bubble at the first appropriate opportunity.
// This bubble may be delayed by other UI, like global errors and sync promos.
class FirstRunBubbleLauncher : public content::NotificationObserver {
 public:
  // Show the bubble at the first appropriate opportunity. This function
  // instantiates a FirstRunBubbleLauncher, which manages its own lifetime.
  static void ShowFirstRunBubbleSoon();

 private:
  FirstRunBubbleLauncher();
  ~FirstRunBubbleLauncher() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleLauncher);
};

// static
void FirstRunBubbleLauncher::ShowFirstRunBubbleSoon() {
  SetShowFirstRunBubblePref(first_run::FIRST_RUN_BUBBLE_SHOW);
  // This FirstRunBubbleLauncher instance will manage its own lifetime.
  new FirstRunBubbleLauncher();
}

FirstRunBubbleLauncher::FirstRunBubbleLauncher() {
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::NotificationService::AllSources());

  // This notification is required to observe the switch between the sync setup
  // page and the general settings page.
  registrar_.Add(this, chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
                 content::NotificationService::AllSources());
}

FirstRunBubbleLauncher::~FirstRunBubbleLauncher() {}

void FirstRunBubbleLauncher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME ||
         type == chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED);

  Browser* browser = chrome::FindBrowserWithWebContents(
      content::Source<content::WebContents>(source).ptr());
  if (!browser || !browser->is_type_tabbed())
    return;

  // Check the preference to determine if the bubble should be shown.
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs || prefs->GetInteger(prefs::kShowFirstRunBubbleOption) !=
      first_run::FIRST_RUN_BUBBLE_SHOW) {
    delete this;
    return;
  }

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Suppress the first run bubble if a Gaia sign in page or the sync setup
  // page is showing.
  if (contents && (contents->GetURL().GetOrigin().spec() ==
                       chrome::kChromeUIChromeSigninURL ||
                   gaia::IsGaiaSignonRealm(contents->GetURL().GetOrigin()) ||
                   contents->GetURL() ==
                       chrome::GetSettingsUrl(chrome::kSyncSetupSubPage) ||
                   first_run::IsOnWelcomePage(contents))) {
    return;
  }

  if (contents && contents->GetURL().SchemeIs(content::kChromeUIScheme)) {
#if defined(OS_WIN)
    // Suppress the first run bubble if 'make chrome metro' flow is showing.
    if (contents->GetURL().host_piece() == chrome::kChromeUIMetroFlowHost)
      return;
#endif

    // Suppress the first run bubble if the NTP sync promo bubble is showing
    // or if sign in is in progress.
    if (contents->GetURL().host_piece() == chrome::kChromeUINewTabHost) {
      Profile* profile =
          Profile::FromBrowserContext(contents->GetBrowserContext());
      SigninManagerBase* manager =
          SigninManagerFactory::GetForProfile(profile);
      bool signin_in_progress = manager && manager->AuthInProgress();
      bool is_promo_bubble_visible =
          profile->GetPrefs()->GetBoolean(prefs::kSignInPromoShowNTPBubble);

      if (is_promo_bubble_visible || signin_in_progress)
        return;
    }
  }

  // Suppress the first run bubble if a global error bubble is pending.
  GlobalErrorService* global_error_service =
      GlobalErrorServiceFactory::GetForProfile(browser->profile());
  if (global_error_service->GetFirstGlobalErrorWithBubbleView() != NULL)
    return;

  // Reset the preference and notifications to avoid showing the bubble again.
  prefs->SetInteger(prefs::kShowFirstRunBubbleOption,
                    first_run::FIRST_RUN_BUBBLE_DONT_SHOW);

  // Show the bubble now and destroy this bubble launcher.
  browser->ShowFirstRunBubble();
  delete this;
}

static base::LazyInstance<base::FilePath>::DestructorAtExit
    master_prefs_path_for_testing = LAZY_INSTANCE_INITIALIZER;

// Loads master preferences from the master preference file into the installer
// master preferences. Returns the pointer to installer::MasterPreferences
// object if successful; otherwise, returns NULL.
installer::MasterPreferences* LoadMasterPrefs() {
  base::FilePath master_prefs_path;
  if (!master_prefs_path_for_testing.Get().empty()) {
    master_prefs_path = master_prefs_path_for_testing.Get();
  } else {
    master_prefs_path = base::FilePath(first_run::internal::MasterPrefsPath());
  }
  if (master_prefs_path.empty())
    return NULL;
  installer::MasterPreferences* install_prefs =
      new installer::MasterPreferences(master_prefs_path);
  if (!install_prefs->read_from_file()) {
    delete install_prefs;
    return NULL;
  }

  return install_prefs;
}

// Makes chrome the user's default browser according to policy or
// |make_chrome_default_for_user| if no policy is set.
void ProcessDefaultBrowserPolicy(bool make_chrome_default_for_user) {
  // Only proceed if chrome can be made default unattended. In other cases, this
  // is handled by the first run default browser prompt (on Windows 8+).
  if (shell_integration::GetDefaultWebClientSetPermission() ==
      shell_integration::SET_DEFAULT_UNATTENDED) {
    // The policy has precedence over the user's choice.
    if (g_browser_process->local_state()->IsManagedPreference(
            prefs::kDefaultBrowserSettingEnabled)) {
      if (g_browser_process->local_state()->GetBoolean(
          prefs::kDefaultBrowserSettingEnabled)) {
        shell_integration::SetAsDefaultBrowser();
      }
    } else if (make_chrome_default_for_user) {
      shell_integration::SetAsDefaultBrowser();
    }
  }
}

}  // namespace

namespace first_run {
namespace internal {

void SetupMasterPrefsFromInstallPrefs(
    const installer::MasterPreferences& install_prefs,
    MasterPrefs* out_prefs) {
  ConvertStringVectorToGURLVector(
      install_prefs.GetFirstRunTabs(), &out_prefs->new_tabs);

  // If we're suppressing the first-run bubble, set that preference now.
  // Otherwise, wait until the user has completed first run to set it, so the
  // user is guaranteed to see the bubble iff they have completed the first run
  // process.
  bool value = false;
  if (install_prefs.GetBool(
          installer::master_preferences::kDistroSuppressFirstRunBubble,
          &value) &&
      value) {
    SetShowFirstRunBubblePref(FIRST_RUN_BUBBLE_SUPPRESS);
  }

  if (install_prefs.GetBool(
          installer::master_preferences::kMakeChromeDefaultForUser,
          &value) && value) {
    out_prefs->make_chrome_default_for_user = true;
  }

  if (install_prefs.GetBool(
          installer::master_preferences::kSuppressFirstRunDefaultBrowserPrompt,
          &value) && value) {
    out_prefs->suppress_first_run_default_browser_prompt = true;
  }

  install_prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &out_prefs->import_bookmarks_path);

  out_prefs->compressed_variations_seed =
      install_prefs.GetCompressedVariationsSeed();
  out_prefs->variations_seed_signature =
      install_prefs.GetVariationsSeedSignature();

  install_prefs.GetString(
      installer::master_preferences::kDistroSuppressDefaultBrowserPromptPref,
      &out_prefs->suppress_default_browser_prompt_for_version);

  if (install_prefs.GetBool(
          installer::master_preferences::kDistroWelcomePageOnOSUpgradeEnabled,
          &value) &&
      !value) {
    out_prefs->welcome_page_on_os_upgrade_enabled = false;
  }
}

bool GetFirstRunSentinelFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  *path = user_data_dir.Append(chrome::kFirstRunSentinel);
  return true;
}

bool CreateSentinel() {
  base::FilePath first_run_sentinel;
  return GetFirstRunSentinelFilePath(&first_run_sentinel) &&
      base::WriteFile(first_run_sentinel, "", 0) != -1;
}

// -- Platform-specific functions --

#if !defined(OS_LINUX) && !defined(OS_BSD)
bool IsOrganicFirstRun() {
  std::string brand;
  google_brand::GetBrand(&brand);
  return google_brand::IsOrganicFirstRun(brand);
}
#endif

FirstRunState DetermineFirstRunState(bool has_sentinel,
                                     bool force_first_run,
                                     bool no_first_run) {
  return (force_first_run || (!has_sentinel && !no_first_run))
             ? FIRST_RUN_TRUE
             : FIRST_RUN_FALSE;
}

}  // namespace internal

MasterPrefs::MasterPrefs() = default;

MasterPrefs::~MasterPrefs() = default;

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportAutofillFormData, false);
  registry->RegisterBooleanPref(prefs::kImportBookmarks, false);
  registry->RegisterBooleanPref(prefs::kImportHistory, false);
  registry->RegisterBooleanPref(prefs::kImportHomepage, false);
  registry->RegisterBooleanPref(prefs::kImportSavedPasswords, false);
  registry->RegisterBooleanPref(prefs::kImportSearchEngine, false);
}

bool IsChromeFirstRun() {
  if (g_first_run == internal::FIRST_RUN_UNKNOWN) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    g_first_run = internal::DetermineFirstRunState(
        internal::IsFirstRunSentinelPresent(),
        command_line->HasSwitch(switches::kForceFirstRun),
        command_line->HasSwitch(switches::kNoFirstRun));
  }
  return g_first_run == internal::FIRST_RUN_TRUE;
}

#if defined(OS_MACOSX)
bool IsFirstRunSuppressed(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kNoFirstRun);
}
#endif

bool IsMetricsReportingOptIn() {
  // Metrics reporting is opt-out by default for all platforms and channels.
  // However, user will have chance to modify metrics reporting state during
  // first run.
  return false;
}

void CreateSentinelIfNeeded() {
  if (IsChromeFirstRun())
    internal::CreateSentinel();
}

base::Time GetFirstRunSentinelCreationTime() {
  base::FilePath first_run_sentinel;
  base::Time first_run_sentinel_creation_time = base::Time();
  if (first_run::internal::GetFirstRunSentinelFilePath(&first_run_sentinel)) {
    base::File::Info info;
    if (base::GetFileInfo(first_run_sentinel, &info))
      first_run_sentinel_creation_time = info.creation_time;
  }
  return first_run_sentinel_creation_time;
}

bool SetShowFirstRunBubblePref(FirstRunBubbleOptions show_bubble_option) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  if (local_state->GetInteger(
          prefs::kShowFirstRunBubbleOption) != FIRST_RUN_BUBBLE_SUPPRESS) {
    // Set the new state as long as the bubble wasn't explicitly suppressed
    // already.
    local_state->SetInteger(prefs::kShowFirstRunBubbleOption,
                            show_bubble_option);
  }
  return true;
}

void SetShouldShowWelcomePage() {
  g_should_show_welcome_page = true;
}

bool ShouldShowWelcomePage() {
  bool retval = g_should_show_welcome_page;
  g_should_show_welcome_page = false;
  return retval;
}

bool IsOnWelcomePage(content::WebContents* contents) {
  const GURL welcome_page(chrome::kChromeUIWelcomeURL);
  const GURL welcome_page_win10(chrome::kChromeUIWelcomeWin10URL);
  const GURL current = contents->GetURL().GetWithEmptyPath();
  return current == welcome_page || current == welcome_page_win10;
}

void SetShouldDoPersonalDataManagerFirstRun() {
  g_should_do_autofill_personal_data_manager_first_run = true;
}

bool ShouldDoPersonalDataManagerFirstRun() {
  bool retval = g_should_do_autofill_personal_data_manager_first_run;
  g_should_do_autofill_personal_data_manager_first_run = false;
  return retval;
}

void LogFirstRunMetric(FirstRunBubbleMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("FirstRun.SearchEngineBubble", metric,
                            NUM_FIRST_RUN_BUBBLE_METRICS);
}

void SetMasterPrefsPathForTesting(const base::FilePath& master_prefs) {
  master_prefs_path_for_testing.Get() = master_prefs;
}

ProcessMasterPreferencesResult ProcessMasterPreferences(
    const base::FilePath& user_data_dir,
    MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  std::unique_ptr<installer::MasterPreferences> install_prefs(
      LoadMasterPrefs());

  if (install_prefs.get()) {
    if (!internal::ShowPostInstallEULAIfNeeded(install_prefs.get()))
      return EULA_EXIT_NOW;

    std::unique_ptr<base::DictionaryValue> master_dictionary =
        install_prefs->master_dictionary().CreateDeepCopy();
    // The distribution dictionary (and any prefs below it) are never registered
    // for use in Chrome's PrefService. Strip them from the master dictionary
    // before mapping it to prefs.
    master_dictionary->RemoveWithoutPathExpansion(
        installer::master_preferences::kDistroDict, nullptr);

    if (!chrome_prefs::InitializePrefsFromMasterPrefs(
            profiles::GetDefaultProfileDir(user_data_dir),
            std::move(master_dictionary))) {
      DLOG(ERROR) << "Failed to initialize from master_preferences.";
    }

    DoDelayedInstallExtensionsIfNeeded(install_prefs.get());

    internal::SetupMasterPrefsFromInstallPrefs(*install_prefs, out_prefs);
  }

  return FIRST_RUN_PROCEED;
}

void AutoImport(
    Profile* profile,
    const std::string& import_bookmarks_path) {
  g_auto_import_state |= AUTO_IMPORT_CALLED;

  // Use |profile|'s PrefService to determine what to import. It will reflect in
  // order:
  //  1) Policies.
  //  2) Master preferences (used to initialize user prefs in
  //     ProcessMasterPreferences()).
  //  3) Recommended policies.
  //  4) Registered default.
  PrefService* prefs = profile->GetPrefs();
  uint16_t items_to_import = 0;
  static constexpr struct {
    const char* pref_path;
    importer::ImportItem bit;
  } kImportItems[] = {
      {prefs::kImportAutofillFormData, importer::AUTOFILL_FORM_DATA},
      {prefs::kImportBookmarks, importer::FAVORITES},
      {prefs::kImportHistory, importer::HISTORY},
      {prefs::kImportHomepage, importer::HOME_PAGE},
      {prefs::kImportSavedPasswords, importer::PASSWORDS},
      {prefs::kImportSearchEngine, importer::SEARCH_ENGINES},
  };

  for (const auto& import_item : kImportItems) {
    if (prefs->GetBoolean(import_item.pref_path))
      items_to_import |= import_item.bit;
  }

  if (items_to_import) {
    // It may be possible to do the if block below asynchronously. In which
    // case, get rid of this RunLoop. http://crbug.com/366116.
    base::RunLoop run_loop;
    auto importer_list = base::MakeUnique<ImporterList>();
    importer_list->DetectSourceProfiles(
        g_browser_process->GetApplicationLocale(),
        false,  // include_interactive_profiles?
        run_loop.QuitClosure());
    run_loop.Run();

    if (importer_list->count() > 0) {
      importer::LogImporterUseToMetrics(
          "AutoImport", importer_list->GetSourceProfileAt(0).importer_type);

      ImportSettings(profile, std::move(importer_list), items_to_import);
    }
  }

  if (!import_bookmarks_path.empty())
    ImportFromFile(profile, import_bookmarks_path);
}

void DoPostImportTasks(Profile* profile, bool make_chrome_default_for_user) {
  // Only set default browser after import as auto import relies on the current
  // default browser to know what to import from.
  ProcessDefaultBrowserPolicy(make_chrome_default_for_user);

  // Display the first run bubble if there is a default search provider.
  TemplateURLService* template_url =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url && template_url->GetDefaultSearchProvider())
    FirstRunBubbleLauncher::ShowFirstRunBubbleSoon();
  SetShouldShowWelcomePage();
  SetShouldDoPersonalDataManagerFirstRun();

  internal::DoPostImportPlatformSpecificTasks(profile);
}

uint16_t auto_import_state() {
  return g_auto_import_state;
}

}  // namespace first_run
