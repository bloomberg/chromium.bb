// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/custodian_profile_downloader_service.h"
#include "chrome/browser/supervised_user/custodian_profile_downloader_service_factory.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist_downloader.h"
#include "chrome/browser/supervised_user/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/permission_request_creator_sync.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_pref_mapping_service.h"
#include "chrome/browser/supervised_user/supervised_user_pref_mapping_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/supervised_user_private/supervised_user_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#endif

#if defined(ENABLE_THEMES)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

using base::DictionaryValue;
using base::UserMetricsAction;
using content::BrowserThread;

SupervisedUserService::URLFilterContext::URLFilterContext()
    : ui_url_filter_(new SupervisedUserURLFilter),
      io_url_filter_(new SupervisedUserURLFilter) {}
SupervisedUserService::URLFilterContext::~URLFilterContext() {}

SupervisedUserURLFilter*
SupervisedUserService::URLFilterContext::ui_url_filter() const {
  return ui_url_filter_.get();
}

SupervisedUserURLFilter*
SupervisedUserService::URLFilterContext::io_url_filter() const {
  return io_url_filter_.get();
}

void SupervisedUserService::URLFilterContext::SetDefaultFilteringBehavior(
    SupervisedUserURLFilter::FilteringBehavior behavior) {
  ui_url_filter_->SetDefaultFilteringBehavior(behavior);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::SetDefaultFilteringBehavior,
                 io_url_filter_.get(), behavior));
}

void SupervisedUserService::URLFilterContext::LoadWhitelists(
    ScopedVector<SupervisedUserSiteList> site_lists) {
  // SupervisedUserURLFilter::LoadWhitelists takes ownership of |site_lists|,
  // so we make an additional copy of it.
  /// TODO(bauerb): This is kinda ugly.
  ScopedVector<SupervisedUserSiteList> site_lists_copy;
  for (ScopedVector<SupervisedUserSiteList>::iterator it = site_lists.begin();
       it != site_lists.end(); ++it) {
    site_lists_copy.push_back((*it)->Clone());
  }
  ui_url_filter_->LoadWhitelists(site_lists.Pass());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::LoadWhitelists,
                 io_url_filter_, base::Passed(&site_lists_copy)));
}

void SupervisedUserService::URLFilterContext::LoadBlacklist(
    const base::FilePath& path) {
  // For now, support loading only once. If we want to support re-load, we'll
  // have to clear the blacklist pointer in the url filters first.
  DCHECK_EQ(0u, blacklist_.GetEntryCount());
  blacklist_.ReadFromFile(
      path,
      base::Bind(&SupervisedUserService::URLFilterContext::OnBlacklistLoaded,
                 base::Unretained(this)));
}

void SupervisedUserService::URLFilterContext::SetManualHosts(
    scoped_ptr<std::map<std::string, bool> > host_map) {
  ui_url_filter_->SetManualHosts(host_map.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::SetManualHosts,
                 io_url_filter_, base::Owned(host_map.release())));
}

void SupervisedUserService::URLFilterContext::SetManualURLs(
    scoped_ptr<std::map<GURL, bool> > url_map) {
  ui_url_filter_->SetManualURLs(url_map.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::SetManualURLs,
                 io_url_filter_, base::Owned(url_map.release())));
}

void SupervisedUserService::URLFilterContext::OnBlacklistLoaded() {
  ui_url_filter_->SetBlacklist(&blacklist_);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::SetBlacklist,
                 io_url_filter_,
                 &blacklist_));
}

SupervisedUserService::SupervisedUserService(Profile* profile)
    : profile_(profile),
      active_(false),
      delegate_(NULL),
#if defined(ENABLE_EXTENSIONS)
      extension_registry_observer_(this),
#endif
      waiting_for_sync_initialization_(false),
      is_profile_active_(false),
      elevated_for_testing_(false),
      did_init_(false),
      did_shutdown_(false),
      weak_ptr_factory_(this) {
}

SupervisedUserService::~SupervisedUserService() {
  DCHECK(!did_init_ || did_shutdown_);
}

void SupervisedUserService::Shutdown() {
  if (!did_init_)
    return;
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  if (ProfileIsSupervised()) {
    content::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
  SetActive(false);

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Can be null in tests.
  if (sync_service)
    sync_service->RemovePreferenceProvider(this);
}

bool SupervisedUserService::ProfileIsSupervised() const {
  return profile_->IsSupervised();
}

void SupervisedUserService::OnCustodianInfoChanged() {
  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnCustodianInfoChanged());
}

// static
void SupervisedUserService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserManualHosts,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserManualURLs,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      SupervisedUserURLFilter::ALLOW,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserCustodianEmail, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserCustodianName, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserCustodianProfileImageURL, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserCustodianProfileURL, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserSecondCustodianEmail, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserSecondCustodianName, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserSecondCustodianProfileImageURL, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserSecondCustodianProfileURL, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSupervisedUserCreationAllowed, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void SupervisedUserService::SetDelegate(Delegate* delegate) {
  if (delegate) {
    // Changing delegates isn't allowed.
    DCHECK(!delegate_);
  } else {
    // If the delegate is removed, deactivate first to give the old delegate a
    // chance to clean up.
    SetActive(false);
  }
  delegate_ = delegate;
}

scoped_refptr<const SupervisedUserURLFilter>
SupervisedUserService::GetURLFilterForIOThread() {
  return url_filter_context_.io_url_filter();
}

SupervisedUserURLFilter* SupervisedUserService::GetURLFilterForUIThread() {
  return url_filter_context_.ui_url_filter();
}

// Items not on any list must return -1 (CATEGORY_NOT_ON_LIST in history.js).
// Items on a list, but with no category, must return 0 (CATEGORY_OTHER).
#define CATEGORY_NOT_ON_LIST -1;
#define CATEGORY_OTHER 0;

int SupervisedUserService::GetCategory(const GURL& url) {
  std::vector<SupervisedUserSiteList::Site*> sites;
  GetURLFilterForUIThread()->GetSites(url, &sites);
  if (sites.empty())
    return CATEGORY_NOT_ON_LIST;

  return (*sites.begin())->category_id;
}

// static
void SupervisedUserService::GetCategoryNames(CategoryList* list) {
  SupervisedUserSiteList::GetCategoryNames(list);
}

std::string SupervisedUserService::GetCustodianEmailAddress() const {
#if defined(OS_CHROMEOS)
  return chromeos::ChromeUserManager::Get()
      ->GetSupervisedUserManager()
      ->GetManagerDisplayEmail(
          user_manager::UserManager::Get()->GetActiveUser()->email());
#else
  return profile_->GetPrefs()->GetString(prefs::kSupervisedUserCustodianEmail);
#endif
}

std::string SupervisedUserService::GetCustodianName() const {
#if defined(OS_CHROMEOS)
  return base::UTF16ToUTF8(
      chromeos::ChromeUserManager::Get()
          ->GetSupervisedUserManager()
          ->GetManagerDisplayName(
              user_manager::UserManager::Get()->GetActiveUser()->email()));
#else
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianName);
  return name.empty() ? GetCustodianEmailAddress() : name;
#endif
}

void SupervisedUserService::AddNavigationBlockedCallback(
    const NavigationBlockedCallback& callback) {
  navigation_blocked_callbacks_.push_back(callback);
}

void SupervisedUserService::DidBlockNavigation(
    content::WebContents* web_contents) {
  for (std::vector<NavigationBlockedCallback>::iterator it =
           navigation_blocked_callbacks_.begin();
       it != navigation_blocked_callbacks_.end(); ++it) {
    it->Run(web_contents);
  }
}

void SupervisedUserService::AddObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SupervisedUserService::RemoveObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

#if defined(ENABLE_EXTENSIONS)
std::string SupervisedUserService::GetDebugPolicyProviderName() const {
  // Save the string space in official builds.
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "Supervised User Service";
#endif
}

bool SupervisedUserService::UserMayLoad(const extensions::Extension* extension,
                                        base::string16* error) const {
  base::string16 tmp_error;
  if (ExtensionManagementPolicyImpl(extension, &tmp_error))
    return true;

  bool was_installed_by_default = extension->was_installed_by_default();
  bool was_installed_by_custodian = extension->was_installed_by_custodian();
#if defined(OS_CHROMEOS)
  // On Chrome OS all external sources are controlled by us so it means that
  // they are "default". Method was_installed_by_default returns false because
  // extensions creation flags are ignored in case of default extensions with
  // update URL(the flags aren't passed to OnExternalExtensionUpdateUrlFound).
  // TODO(dpolukhin): remove this Chrome OS specific code as soon as creation
  // flags are not ignored.
  was_installed_by_default =
      extensions::Manifest::IsExternalLocation(extension->location());
#endif
  if (extensions::Manifest::IsComponentLocation(extension->location()) ||
      was_installed_by_default ||
      was_installed_by_custodian) {
    return true;
  }

  if (error)
    *error = tmp_error;
  return false;
}

bool SupervisedUserService::UserMayModifySettings(
    const extensions::Extension* extension,
    base::string16* error) const {
  return ExtensionManagementPolicyImpl(extension, error);
}

void SupervisedUserService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!extensions::SupervisedUserInfo::GetContentPackSiteList(extension)
           .empty()) {
    UpdateSiteLists();
  }
}
void SupervisedUserService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (!extensions::SupervisedUserInfo::GetContentPackSiteList(extension)
           .empty()) {
    UpdateSiteLists();
  }
}
#endif  // defined(ENABLE_EXTENSIONS)

syncer::ModelTypeSet SupervisedUserService::GetPreferredDataTypes() const {
  if (!ProfileIsSupervised())
    return syncer::ModelTypeSet();

  syncer::ModelTypeSet result;
  result.Put(syncer::SESSIONS);
  result.Put(syncer::EXTENSIONS);
  result.Put(syncer::EXTENSION_SETTINGS);
  result.Put(syncer::APPS);
  result.Put(syncer::APP_SETTINGS);
  result.Put(syncer::APP_NOTIFICATIONS);
  result.Put(syncer::APP_LIST);
  return result;
}

void SupervisedUserService::OnStateChanged() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (waiting_for_sync_initialization_ && service->sync_initialized() &&
      service->backend_mode() == ProfileSyncService::SYNC) {
    waiting_for_sync_initialization_ = false;
    service->RemoveObserver(this);
    FinishSetupSync();
    return;
  }

  DLOG_IF(ERROR, service->GetAuthError().state() ==
                     GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)
      << "Credentials rejected";
}

void SupervisedUserService::SetupSync() {
  StartSetupSync();
  FinishSetupSyncWhenReady();
}

void SupervisedUserService::StartSetupSync() {
  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  ProfileSyncServiceFactory::GetForProfile(profile_)->SetSetupInProgress(true);
}

void SupervisedUserService::FinishSetupSyncWhenReady() {
  // If we're already waiting for the Sync backend, there's nothing to do here.
  if (waiting_for_sync_initialization_)
    return;

  // Continue in FinishSetupSync() once the Sync backend has been initialized.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service->sync_initialized() &&
      service->backend_mode() == ProfileSyncService::SYNC) {
    FinishSetupSync();
  } else {
    service->AddObserver(this);
    waiting_for_sync_initialization_ = true;
  }
}

void SupervisedUserService::FinishSetupSync() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  DCHECK(service->sync_initialized() &&
         service->backend_mode() == ProfileSyncService::SYNC);

  // Sync nothing (except types which are set via GetPreferredDataTypes).
  bool sync_everything = false;
  syncer::ModelTypeSet synced_datatypes;
  service->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Notify ProfileSyncService that we are done with configuration.
  service->SetSetupInProgress(false);
  service->SetSyncSetupCompleted();
}

#if defined(ENABLE_EXTENSIONS)
bool SupervisedUserService::ExtensionManagementPolicyImpl(
    const extensions::Extension* extension,
    base::string16* error) const {
  // |extension| can be NULL in unit_tests.
  if (!ProfileIsSupervised() || (extension && extension->is_theme()))
    return true;

  if (elevated_for_testing_)
    return true;

  if (error)
    *error = l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER);
  return false;
}

ScopedVector<SupervisedUserSiteList>
SupervisedUserService::GetActiveSiteLists() {
  ScopedVector<SupervisedUserSiteList> site_lists;
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  // Can be NULL in unit tests.
  if (!extension_service)
    return site_lists.Pass();

  const extensions::ExtensionSet* extensions = extension_service->extensions();
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const extensions::Extension* extension = it->get();
    if (!extension_service->IsExtensionEnabled(extension->id()))
      continue;

    extensions::ExtensionResource site_list =
        extensions::SupervisedUserInfo::GetContentPackSiteList(extension);
    if (!site_list.empty()) {
      site_lists.push_back(new SupervisedUserSiteList(extension->id(),
                                                      site_list.GetFilePath()));
    }
  }

  return site_lists.Pass();
}

void SupervisedUserService::SetExtensionsActive() {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ManagementPolicy* management_policy =
      extension_system->management_policy();

  if (active_) {
    if (management_policy)
      management_policy->RegisterProvider(this);

    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(profile_));
  } else {
    if (management_policy)
      management_policy->UnregisterProvider(this);

    extension_registry_observer_.RemoveAll();
  }
}
#endif  // defined(ENABLE_EXTENSIONS)

SupervisedUserSettingsService* SupervisedUserService::GetSettingsService() {
  return SupervisedUserSettingsServiceFactory::GetForProfile(profile_);
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  std::string supervised_user_id =
      profile_->GetPrefs()->GetString(prefs::kSupervisedUserId);
  SetActive(!supervised_user_id.empty());
}

void SupervisedUserService::OnDefaultFilteringBehaviorChanged() {
  DCHECK(ProfileIsSupervised());

  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);
  url_filter_context_.SetDefaultFilteringBehavior(behavior);

  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
}

void SupervisedUserService::UpdateSiteLists() {
#if defined(ENABLE_EXTENSIONS)
  url_filter_context_.LoadWhitelists(GetActiveSiteLists());

  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
#endif
}

void SupervisedUserService::LoadBlacklist(const base::FilePath& path,
                                          const GURL& url) {
  if (!url.is_valid()) {
    LoadBlacklistFromFile(path);
    return;
  }

  DCHECK(!blacklist_downloader_.get());
  blacklist_downloader_.reset(new SupervisedUserBlacklistDownloader(
      url,
      path,
      profile_->GetRequestContext(),
      base::Bind(&SupervisedUserService::OnBlacklistDownloadDone,
                 base::Unretained(this), path)));
}

void SupervisedUserService::LoadBlacklistFromFile(const base::FilePath& path) {
  url_filter_context_.LoadBlacklist(path);

  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
}

void SupervisedUserService::OnBlacklistDownloadDone(const base::FilePath& path,
                                                    bool success) {
  if (success) {
    LoadBlacklistFromFile(path);
  } else {
    LOG(WARNING) << "Blacklist download failed";
  }
  blacklist_downloader_.reset();
}

bool SupervisedUserService::AccessRequestsEnabled() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  GoogleServiceAuthError::State state = service->GetAuthError().state();
  // We allow requesting access if Sync is working or has a transient error.
  return (state == GoogleServiceAuthError::NONE ||
          state == GoogleServiceAuthError::CONNECTION_FAILED ||
          state == GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void SupervisedUserService::OnPermissionRequestIssued() {
  // TODO(akuegel): Figure out how to show the result of issuing the permission
  // request in the UI. Currently, we assume the permission request was created
  // successfully.
}

void SupervisedUserService::AddAccessRequest(const GURL& url) {
  permissions_creator_->CreatePermissionRequest(
      SupervisedUserURLFilter::Normalize(url),
      base::Bind(&SupervisedUserService::OnPermissionRequestIssued,
                 weak_ptr_factory_.GetWeakPtr()));
}

SupervisedUserService::ManualBehavior
SupervisedUserService::GetManualBehaviorForHost(
    const std::string& hostname) {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualHosts);
  bool allow = false;
  if (!dict->GetBooleanWithoutPathExpansion(hostname, &allow))
    return MANUAL_NONE;

  return allow ? MANUAL_ALLOW : MANUAL_BLOCK;
}

SupervisedUserService::ManualBehavior
SupervisedUserService::GetManualBehaviorForURL(
    const GURL& url) {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualURLs);
  GURL normalized_url = SupervisedUserURLFilter::Normalize(url);
  bool allow = false;
  if (!dict->GetBooleanWithoutPathExpansion(normalized_url.spec(), &allow))
    return MANUAL_NONE;

  return allow ? MANUAL_ALLOW : MANUAL_BLOCK;
}

void SupervisedUserService::GetManualExceptionsForHost(
    const std::string& host,
    std::vector<GURL>* urls) {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualURLs);
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    GURL url(it.key());
    if (url.host() == host)
      urls->push_back(url);
  }
}

void SupervisedUserService::InitSync(const std::string& refresh_token) {
  StartSetupSync();

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(supervised_users::kSupervisedUserPseudoEmail,
                                   refresh_token);

  FinishSetupSyncWhenReady();
}

void SupervisedUserService::Init() {
  DCHECK(!did_init_);
  did_init_ = true;
  DCHECK(GetSettingsService()->IsReady());

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::Bind(&SupervisedUserService::OnSupervisedUserIdChanged,
          base::Unretained(this)));

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Can be null in tests.
  if (sync_service)
    sync_service->AddPreferenceProvider(this);

  SetActive(ProfileIsSupervised());
}

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;

  if (!delegate_ || !delegate_->SetActive(active_)) {
    if (active_) {
      SupervisedUserPrefMappingServiceFactory::GetForBrowserContext(profile_)
          ->Init();

      CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(switches::kSupervisedUserSyncToken)) {
        InitSync(
            command_line->GetSwitchValueASCII(
                switches::kSupervisedUserSyncToken));
      }

      ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
      token_service->LoadCredentials(
          supervised_users::kSupervisedUserPseudoEmail);

      SetupSync();
    }
  }

  // Now activate/deactivate anything not handled by the delegate yet.

#if defined(ENABLE_THEMES)
  // Re-set the default theme to turn the SU theme on/off.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingDefaultTheme() || theme_service->UsingSystemTheme()) {
    ThemeServiceFactory::GetForProfile(profile_)->UseDefaultTheme();
  }
#endif

  SupervisedUserSettingsService* settings_service = GetSettingsService();
  settings_service->SetActive(active_);

#if defined(ENABLE_EXTENSIONS)
  SetExtensionsActive();
#endif

  if (active_) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kPermissionRequestApiUrl)) {
      permissions_creator_ =
          PermissionRequestCreatorApiary::CreateWithProfile(profile_);
    } else {
      PrefService* pref_service = profile_->GetPrefs();
      permissions_creator_.reset(new PermissionRequestCreatorSync(
          settings_service,
          SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
              profile_),
          GetSupervisedUserName(),
          pref_service->GetString(prefs::kSupervisedUserId)));
    }

    pref_change_registrar_.Add(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        base::Bind(&SupervisedUserService::OnDefaultFilteringBehaviorChanged,
            base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserManualHosts,
        base::Bind(&SupervisedUserService::UpdateManualHosts,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserManualURLs,
        base::Bind(&SupervisedUserService::UpdateManualURLs,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserCustodianName,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserCustodianEmail,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserCustodianProfileImageURL,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserCustodianProfileURL,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserSecondCustodianName,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserSecondCustodianEmail,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserSecondCustodianProfileImageURL,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kSupervisedUserSecondCustodianProfileURL,
        base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                   base::Unretained(this)));

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    UpdateSiteLists();
    UpdateManualHosts();
    UpdateManualURLs();
    bool use_blacklist =
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableSupervisedUserBlacklist);
    if (delegate_ && use_blacklist) {
      base::FilePath blacklist_path = delegate_->GetBlacklistPath();
      if (!blacklist_path.empty())
        LoadBlacklist(blacklist_path, delegate_->GetBlacklistURL());
    }

#if !defined(OS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::AddObserver(this);
#endif
  } else {
    permissions_creator_.reset();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualHosts);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualURLs);

    if (waiting_for_sync_initialization_)
      ProfileSyncServiceFactory::GetForProfile(profile_)->RemoveObserver(this);

#if !defined(OS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::RemoveObserver(this);
#endif
  }
}

void SupervisedUserService::RegisterAndInitSync(
    SupervisedUserRegistrationUtility* registration_utility,
    Profile* custodian_profile,
    const std::string& supervised_user_id,
    const AuthErrorCallback& callback) {
  DCHECK(ProfileIsSupervised());
  DCHECK(!custodian_profile->IsSupervised());

  base::string16 name = base::UTF8ToUTF16(
      profile_->GetPrefs()->GetString(prefs::kProfileName));
  int avatar_index = profile_->GetPrefs()->GetInteger(
      prefs::kProfileAvatarIndex);
  SupervisedUserRegistrationInfo info(name, avatar_index);
  registration_utility->Register(
      supervised_user_id,
      info,
      base::Bind(&SupervisedUserService::OnSupervisedUserRegistered,
                 weak_ptr_factory_.GetWeakPtr(), callback, custodian_profile));

  // Fetch the custodian's profile information, to store the name.
  // TODO(pamg): If --google-profile-info (flag: switches::kGoogleProfileInfo)
  // is ever enabled, take the name from the ProfileInfoCache instead.
  CustodianProfileDownloaderService* profile_downloader_service =
      CustodianProfileDownloaderServiceFactory::GetForProfile(
          custodian_profile);
  profile_downloader_service->DownloadProfile(
      base::Bind(&SupervisedUserService::OnCustodianProfileDownloaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserService::OnCustodianProfileDownloaded(
    const base::string16& full_name) {
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianName,
                                  base::UTF16ToUTF8(full_name));
}

void SupervisedUserService::OnSupervisedUserRegistered(
    const AuthErrorCallback& callback,
    Profile* custodian_profile,
    const GoogleServiceAuthError& auth_error,
    const std::string& token) {
  if (auth_error.state() == GoogleServiceAuthError::NONE) {
    InitSync(token);
    SigninManagerBase* signin =
        SigninManagerFactory::GetForProfile(custodian_profile);
    profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianEmail,
                                    signin->GetAuthenticatedUsername());

    // The supervised user profile is now ready for use.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
    cache.SetIsOmittedProfileAtIndex(index, false);
  } else {
    DCHECK_EQ(std::string(), token);
  }

  callback.Run(auth_error);
}

void SupervisedUserService::UpdateManualHosts() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualHosts);
  scoped_ptr<std::map<std::string, bool> > host_map(
      new std::map<std::string, bool>());
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    (*host_map)[it.key()] = allow;
  }
  url_filter_context_.SetManualHosts(host_map.Pass());

  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
}

void SupervisedUserService::UpdateManualURLs() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualURLs);
  scoped_ptr<std::map<GURL, bool> > url_map(new std::map<GURL, bool>());
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    (*url_map)[GURL(it.key())] = allow;
  }
  url_filter_context_.SetManualURLs(url_map.Pass());

  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
}

void SupervisedUserService::OnBrowserSetLastActive(Browser* browser) {
  bool profile_became_active = profile_->IsSameProfile(browser->profile());
  if (!is_profile_active_ && profile_became_active)
    content::RecordAction(UserMetricsAction("ManagedUsers_OpenProfile"));
  else if (is_profile_active_ && !profile_became_active)
    content::RecordAction(UserMetricsAction("ManagedUsers_SwitchProfile"));

  is_profile_active_ = profile_became_active;
}

std::string SupervisedUserService::GetSupervisedUserName() const {
#if defined(OS_CHROMEOS)
  // The active user can be NULL in unit tests.
  if (user_manager::UserManager::Get()->GetActiveUser()) {
    return UTF16ToUTF8(user_manager::UserManager::Get()->GetUserDisplayName(
        user_manager::UserManager::Get()->GetActiveUser()->GetUserID()));
  }
  return std::string();
#else
  return profile_->GetPrefs()->GetString(prefs::kProfileName);
#endif
}
