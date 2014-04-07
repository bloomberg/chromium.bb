// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_service.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/managed_mode/custodian_profile_downloader_service.h"
#include "chrome/browser/managed_mode/custodian_profile_downloader_service_factory.h"
#include "chrome/browser/managed_mode/managed_mode_site_list.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility.h"
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/browser/managed_mode/supervised_user_pref_mapping_service.h"
#include "chrome/browser/managed_mode/supervised_user_pref_mapping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/managed_mode_private/managed_mode_handler.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using base::DictionaryValue;
using base::UserMetricsAction;
using content::BrowserThread;

const char kManagedUserAccessRequestKeyPrefix[] =
    "X-ManagedUser-AccessRequests";
const char kManagedUserAccessRequestTime[] = "timestamp";
const char kManagedUserName[] = "name";

// Key for the notification setting of the custodian. This is a shared setting
// so we can include the setting in the access request data that is used to
// trigger notifications.
const char kNotificationSetting[] = "custodian-notification-setting";

ManagedUserService::URLFilterContext::URLFilterContext()
    : ui_url_filter_(new ManagedModeURLFilter),
      io_url_filter_(new ManagedModeURLFilter) {}
ManagedUserService::URLFilterContext::~URLFilterContext() {}

ManagedModeURLFilter*
ManagedUserService::URLFilterContext::ui_url_filter() const {
  return ui_url_filter_.get();
}

ManagedModeURLFilter*
ManagedUserService::URLFilterContext::io_url_filter() const {
  return io_url_filter_.get();
}

void ManagedUserService::URLFilterContext::SetDefaultFilteringBehavior(
    ManagedModeURLFilter::FilteringBehavior behavior) {
  ui_url_filter_->SetDefaultFilteringBehavior(behavior);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetDefaultFilteringBehavior,
                 io_url_filter_.get(), behavior));
}

void ManagedUserService::URLFilterContext::LoadWhitelists(
    ScopedVector<ManagedModeSiteList> site_lists) {
  // ManagedModeURLFilter::LoadWhitelists takes ownership of |site_lists|,
  // so we make an additional copy of it.
  /// TODO(bauerb): This is kinda ugly.
  ScopedVector<ManagedModeSiteList> site_lists_copy;
  for (ScopedVector<ManagedModeSiteList>::iterator it = site_lists.begin();
       it != site_lists.end(); ++it) {
    site_lists_copy.push_back((*it)->Clone());
  }
  ui_url_filter_->LoadWhitelists(site_lists.Pass());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::LoadWhitelists,
                 io_url_filter_, base::Passed(&site_lists_copy)));
}

void ManagedUserService::URLFilterContext::SetManualHosts(
    scoped_ptr<std::map<std::string, bool> > host_map) {
  ui_url_filter_->SetManualHosts(host_map.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetManualHosts,
                 io_url_filter_, base::Owned(host_map.release())));
}

void ManagedUserService::URLFilterContext::SetManualURLs(
    scoped_ptr<std::map<GURL, bool> > url_map) {
  ui_url_filter_->SetManualURLs(url_map.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetManualURLs,
                 io_url_filter_, base::Owned(url_map.release())));
}

ManagedUserService::ManagedUserService(Profile* profile)
    : profile_(profile),
      waiting_for_sync_initialization_(false),
      is_profile_active_(false),
      elevated_for_testing_(false),
      did_shutdown_(false),
      weak_ptr_factory_(this) {
}

ManagedUserService::~ManagedUserService() {
  DCHECK(did_shutdown_);
}

void ManagedUserService::Shutdown() {
  did_shutdown_ = true;
  if (ProfileIsManaged()) {
    content::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
#if !defined(OS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::RemoveObserver(this);
#endif
  }

  if (!waiting_for_sync_initialization_)
    return;

  ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
  sync_service->RemoveObserver(this);
}

bool ManagedUserService::ProfileIsManaged() const {
  return profile_->IsManaged();
}

// static
void ManagedUserService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kManagedModeManualHosts,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kManagedModeManualURLs,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kDefaultManagedModeFilteringBehavior, ManagedModeURLFilter::ALLOW,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kManagedUserCustodianEmail, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kManagedUserCustodianName, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kManagedUserCreationAllowed, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
void ManagedUserService::MigrateUserPrefs(PrefService* prefs) {
  if (!prefs->HasPrefPath(prefs::kProfileIsManaged))
    return;

  bool is_managed = prefs->GetBoolean(prefs::kProfileIsManaged);
  prefs->ClearPref(prefs::kProfileIsManaged);

  if (!is_managed)
    return;

  std::string managed_user_id = prefs->GetString(prefs::kManagedUserId);
  if (!managed_user_id.empty())
    return;

  prefs->SetString(prefs::kManagedUserId, "Dummy ID");
}

scoped_refptr<const ManagedModeURLFilter>
ManagedUserService::GetURLFilterForIOThread() {
  return url_filter_context_.io_url_filter();
}

ManagedModeURLFilter* ManagedUserService::GetURLFilterForUIThread() {
  return url_filter_context_.ui_url_filter();
}

// Items not on any list must return -1 (CATEGORY_NOT_ON_LIST in history.js).
// Items on a list, but with no category, must return 0 (CATEGORY_OTHER).
#define CATEGORY_NOT_ON_LIST -1;
#define CATEGORY_OTHER 0;

int ManagedUserService::GetCategory(const GURL& url) {
  std::vector<ManagedModeSiteList::Site*> sites;
  GetURLFilterForUIThread()->GetSites(url, &sites);
  if (sites.empty())
    return CATEGORY_NOT_ON_LIST;

  return (*sites.begin())->category_id;
}

// static
void ManagedUserService::GetCategoryNames(CategoryList* list) {
  ManagedModeSiteList::GetCategoryNames(list);
}

std::string ManagedUserService::GetCustodianEmailAddress() const {
#if defined(OS_CHROMEOS)
  return chromeos::UserManager::Get()->GetSupervisedUserManager()->
      GetManagerDisplayEmail(
          chromeos::UserManager::Get()->GetActiveUser()->email());
#else
  return profile_->GetPrefs()->GetString(prefs::kManagedUserCustodianEmail);
#endif
}

std::string ManagedUserService::GetCustodianName() const {
#if defined(OS_CHROMEOS)
  return base::UTF16ToUTF8(chromeos::UserManager::Get()->
      GetSupervisedUserManager()->GetManagerDisplayName(
          chromeos::UserManager::Get()->GetActiveUser()->email()));
#else
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kManagedUserCustodianName);
  return name.empty() ? GetCustodianEmailAddress() : name;
#endif
}

void ManagedUserService::AddNavigationBlockedCallback(
    const NavigationBlockedCallback& callback) {
  navigation_blocked_callbacks_.push_back(callback);
}

void ManagedUserService::DidBlockNavigation(
    content::WebContents* web_contents) {
  for (std::vector<NavigationBlockedCallback>::iterator it =
           navigation_blocked_callbacks_.begin();
       it != navigation_blocked_callbacks_.end(); ++it) {
    it->Run(web_contents);
  }
}

std::string ManagedUserService::GetDebugPolicyProviderName() const {
  // Save the string space in official builds.
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "Managed User Service";
#endif
}

bool ManagedUserService::UserMayLoad(const extensions::Extension* extension,
                                     base::string16* error) const {
  base::string16 tmp_error;
  if (ExtensionManagementPolicyImpl(extension, &tmp_error))
    return true;

  // If the extension is already loaded, we allow it, otherwise we'd unload
  // all existing extensions.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  // |extension_service| can be NULL in a unit test.
  if (extension_service &&
      extension_service->GetInstalledExtension(extension->id()))
    return true;

  bool was_installed_by_default = extension->was_installed_by_default();
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
  if (extension->location() == extensions::Manifest::COMPONENT ||
      was_installed_by_default) {
    return true;
  }

  if (error)
    *error = tmp_error;
  return false;
}

bool ManagedUserService::UserMayModifySettings(
    const extensions::Extension* extension,
    base::string16* error) const {
  return ExtensionManagementPolicyImpl(extension, error);
}

void ManagedUserService::OnStateChanged() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (waiting_for_sync_initialization_ && service->sync_initialized()) {
    waiting_for_sync_initialization_ = false;
    service->RemoveObserver(this);
    SetupSync();
    return;
  }

  DLOG_IF(ERROR, service->GetAuthError().state() ==
                     GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)
      << "Credentials rejected";
}

void ManagedUserService::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      if (!extensions::ManagedModeInfo::GetContentPackSiteList(
              extension).empty()) {
        UpdateSiteLists();
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      const extensions::UnloadedExtensionInfo* extension_info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      if (!extensions::ManagedModeInfo::GetContentPackSiteList(
              extension_info->extension).empty()) {
        UpdateSiteLists();
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ManagedUserService::SetupSync() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  DCHECK(service->sync_initialized());

  bool sync_everything = false;
  syncer::ModelTypeSet synced_datatypes;
  synced_datatypes.Put(syncer::MANAGED_USER_SETTINGS);
  service->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Notify ProfileSyncService that we are done with configuration.
  service->SetSetupInProgress(false);
  service->SetSyncSetupCompleted();
}

bool ManagedUserService::ExtensionManagementPolicyImpl(
    const extensions::Extension* extension,
    base::string16* error) const {
  // |extension| can be NULL in unit_tests.
  if (!ProfileIsManaged() || (extension && extension->is_theme()))
    return true;

  if (elevated_for_testing_)
    return true;

  if (error)
    *error = l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_MANAGED_USER);
  return false;
}

ScopedVector<ManagedModeSiteList> ManagedUserService::GetActiveSiteLists() {
  ScopedVector<ManagedModeSiteList> site_lists;
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
        extensions::ManagedModeInfo::GetContentPackSiteList(extension);
    if (!site_list.empty()) {
      site_lists.push_back(new ManagedModeSiteList(extension->id(),
                                                   site_list.GetFilePath()));
    }
  }

  return site_lists.Pass();
}

ManagedUserSettingsService* ManagedUserService::GetSettingsService() {
  return ManagedUserSettingsServiceFactory::GetForProfile(profile_);
}

void ManagedUserService::OnDefaultFilteringBehaviorChanged() {
  DCHECK(ProfileIsManaged());

  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultManagedModeFilteringBehavior);
  ManagedModeURLFilter::FilteringBehavior behavior =
      ManagedModeURLFilter::BehaviorFromInt(behavior_value);
  url_filter_context_.SetDefaultFilteringBehavior(behavior);
}

void ManagedUserService::UpdateSiteLists() {
  url_filter_context_.LoadWhitelists(GetActiveSiteLists());
}

bool ManagedUserService::AccessRequestsEnabled() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  GoogleServiceAuthError::State state = service->GetAuthError().state();
  // We allow requesting access if Sync is working or has a transient error.
  return (state == GoogleServiceAuthError::NONE ||
          state == GoogleServiceAuthError::CONNECTION_FAILED ||
          state == GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void ManagedUserService::AddAccessRequest(const GURL& url) {
  // Normalize the URL.
  GURL normalized_url = ManagedModeURLFilter::Normalize(url);

  // Escape the URL.
  std::string output(net::EscapeQueryParamValue(normalized_url.spec(), true));

  // Add the prefix.
  std::string key = ManagedUserSettingsService::MakeSplitSettingKey(
      kManagedUserAccessRequestKeyPrefix, output);

  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  // TODO(sergiu): Use sane time here when it's ready.
  dict->SetDouble(kManagedUserAccessRequestTime, base::Time::Now().ToJsTime());

  dict->SetString(kManagedUserName, profile_->GetProfileName());

  // Copy the notification setting of the custodian.
  std::string managed_user_id =
      profile_->GetPrefs()->GetString(prefs::kManagedUserId);
  const base::Value* value =
      ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(profile_)
          ->GetValue(managed_user_id, kNotificationSetting);
  bool notifications_enabled = false;
  if (value) {
    bool success = value->GetAsBoolean(&notifications_enabled);
    DCHECK(success);
  }
  dict->SetBoolean(kNotificationSetting, notifications_enabled);

  GetSettingsService()->UploadItem(key, dict.PassAs<base::Value>());
}

ManagedUserService::ManualBehavior ManagedUserService::GetManualBehaviorForHost(
    const std::string& hostname) {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualHosts);
  bool allow = false;
  if (!dict->GetBooleanWithoutPathExpansion(hostname, &allow))
    return MANUAL_NONE;

  return allow ? MANUAL_ALLOW : MANUAL_BLOCK;
}

ManagedUserService::ManualBehavior ManagedUserService::GetManualBehaviorForURL(
    const GURL& url) {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualURLs);
  GURL normalized_url = ManagedModeURLFilter::Normalize(url);
  bool allow = false;
  if (!dict->GetBooleanWithoutPathExpansion(normalized_url.spec(), &allow))
    return MANUAL_NONE;

  return allow ? MANUAL_ALLOW : MANUAL_BLOCK;
}

void ManagedUserService::GetManualExceptionsForHost(const std::string& host,
                                                    std::vector<GURL>* urls) {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualURLs);
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    GURL url(it.key());
    if (url.host() == host)
      urls->push_back(url);
  }
}

void ManagedUserService::InitSync(const std::string& refresh_token) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  service->SetSetupInProgress(true);

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(managed_users::kManagedUserPseudoEmail,
                                   refresh_token);

  // Continue in SetupSync() once the Sync backend has been initialized.
  if (service->sync_initialized()) {
    SetupSync();
  } else {
    ProfileSyncServiceFactory::GetForProfile(profile_)->AddObserver(this);
    waiting_for_sync_initialization_ = true;
  }
}

void ManagedUserService::Init() {
  ManagedUserSettingsService* settings_service = GetSettingsService();
  DCHECK(settings_service->IsReady());
  if (!ProfileIsManaged()) {
    settings_service->Clear();
    return;
  }

  settings_service->Activate();

  SupervisedUserPrefMappingServiceFactory::GetForBrowserContext(profile_)
      ->Init();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kManagedUserSyncToken)) {
    InitSync(
        command_line->GetSwitchValueASCII(switches::kManagedUserSyncToken));
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->LoadCredentials(managed_users::kManagedUserPseudoEmail);

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ManagementPolicy* management_policy =
      extension_system->management_policy();
  if (management_policy)
    extension_system->management_policy()->RegisterProvider(this);

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile_));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDefaultManagedModeFilteringBehavior,
      base::Bind(&ManagedUserService::OnDefaultFilteringBehaviorChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kManagedModeManualHosts,
      base::Bind(&ManagedUserService::UpdateManualHosts,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kManagedModeManualURLs,
      base::Bind(&ManagedUserService::UpdateManualURLs,
                 base::Unretained(this)));

#if !defined(OS_ANDROID)
  // TODO(bauerb): Get rid of the platform-specific #ifdef here.
  // http://crbug.com/313377
  BrowserList::AddObserver(this);
#endif

  // Initialize the filter.
  OnDefaultFilteringBehaviorChanged();
  UpdateSiteLists();
  UpdateManualHosts();
  UpdateManualURLs();
}

void ManagedUserService::RegisterAndInitSync(
    ManagedUserRegistrationUtility* registration_utility,
    Profile* custodian_profile,
    const std::string& managed_user_id,
    const AuthErrorCallback& callback) {
  DCHECK(ProfileIsManaged());
  DCHECK(!custodian_profile->IsManaged());

  base::string16 name = base::UTF8ToUTF16(
      profile_->GetPrefs()->GetString(prefs::kProfileName));
  int avatar_index = profile_->GetPrefs()->GetInteger(
      prefs::kProfileAvatarIndex);
  ManagedUserRegistrationInfo info(name, avatar_index);
  registration_utility->Register(
      managed_user_id,
      info,
      base::Bind(&ManagedUserService::OnManagedUserRegistered,
                 weak_ptr_factory_.GetWeakPtr(), callback, custodian_profile));

  // Fetch the custodian's profile information, to store the name.
  // TODO(pamg): If --google-profile-info (flag: switches::kGoogleProfileInfo)
  // is ever enabled, take the name from the ProfileInfoCache instead.
  CustodianProfileDownloaderService* profile_downloader_service =
      CustodianProfileDownloaderServiceFactory::GetForProfile(
          custodian_profile);
  profile_downloader_service->DownloadProfile(
      base::Bind(&ManagedUserService::OnCustodianProfileDownloaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserService::OnCustodianProfileDownloaded(
    const base::string16& full_name) {
  profile_->GetPrefs()->SetString(prefs::kManagedUserCustodianName,
                                  base::UTF16ToUTF8(full_name));
}

void ManagedUserService::OnManagedUserRegistered(
    const AuthErrorCallback& callback,
    Profile* custodian_profile,
    const GoogleServiceAuthError& auth_error,
    const std::string& token) {
  if (auth_error.state() == GoogleServiceAuthError::NONE) {
    InitSync(token);
    SigninManagerBase* signin =
        SigninManagerFactory::GetForProfile(custodian_profile);
    profile_->GetPrefs()->SetString(prefs::kManagedUserCustodianEmail,
                                    signin->GetAuthenticatedUsername());

    // The managed-user profile is now ready for use.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
    cache.SetIsOmittedProfileAtIndex(index, false);
  } else {
    DCHECK_EQ(std::string(), token);
  }

  callback.Run(auth_error);
}

void ManagedUserService::UpdateManualHosts() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualHosts);
  scoped_ptr<std::map<std::string, bool> > host_map(
      new std::map<std::string, bool>());
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    (*host_map)[it.key()] = allow;
  }
  url_filter_context_.SetManualHosts(host_map.Pass());
}

void ManagedUserService::UpdateManualURLs() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualURLs);
  scoped_ptr<std::map<GURL, bool> > url_map(new std::map<GURL, bool>());
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    (*url_map)[GURL(it.key())] = allow;
  }
  url_filter_context_.SetManualURLs(url_map.Pass());
}

void ManagedUserService::OnBrowserSetLastActive(Browser* browser) {
  bool profile_became_active = profile_->IsSameProfile(browser->profile());
  if (!is_profile_active_ && profile_became_active)
    content::RecordAction(UserMetricsAction("ManagedUsers_OpenProfile"));
  else if (is_profile_active_ && !profile_became_active)
    content::RecordAction(UserMetricsAction("ManagedUsers_SwitchProfile"));

  is_profile_active_ = profile_became_active;
}
