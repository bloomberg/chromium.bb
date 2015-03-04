// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist_downloader.h"
#include "chrome/browser/supervised_user/legacy/custodian_profile_downloader_service.h"
#include "chrome/browser/supervised_user/legacy/custodian_profile_downloader_service_factory.h"
#include "chrome/browser/supervised_user/legacy/permission_request_creator_sync.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_pref_mapping_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_pref_mapping_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
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
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#endif

#if defined(ENABLE_THEMES)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

using base::DictionaryValue;
using base::UserMetricsAction;
using content::BrowserThread;

namespace {

const char* const kCustodianInfoPrefs[] = {
  prefs::kSupervisedUserCustodianName,
  prefs::kSupervisedUserCustodianEmail,
  prefs::kSupervisedUserCustodianProfileImageURL,
  prefs::kSupervisedUserCustodianProfileURL,
  prefs::kSupervisedUserSecondCustodianName,
  prefs::kSupervisedUserSecondCustodianEmail,
  prefs::kSupervisedUserSecondCustodianProfileImageURL,
  prefs::kSupervisedUserSecondCustodianProfileURL,
};

void CreateURLAccessRequest(
    const GURL& url,
    PermissionRequestCreator* creator,
    const SupervisedUserService::SuccessCallback& callback) {
  creator->CreateURLAccessRequest(url, callback);
}

void CreateExtensionUpdateRequest(
    const std::string& extension_id,
    PermissionRequestCreator* creator,
    const SupervisedUserService::SuccessCallback& callback) {
  creator->CreateExtensionUpdateRequest(extension_id, callback);
}

#if defined(ENABLE_EXTENSIONS)
enum ExtensionState {
  EXTENSION_FORCED,
  EXTENSION_BLOCKED,
  EXTENSION_ALLOWED
};

ExtensionState GetExtensionState(const extensions::Extension* extension) {
  if (extension->is_theme())
    return EXTENSION_ALLOWED;

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
    // Enforce default extensions as well as custodian-installed extensions
    // (if we'd allow the supervised user to uninstall them, there'd be no way
    // to get them back).
    return EXTENSION_FORCED;
  }

  return EXTENSION_BLOCKED;
}
#endif

}  // namespace

base::FilePath SupervisedUserService::Delegate::GetBlacklistPath() const {
  return base::FilePath();
}

GURL SupervisedUserService::Delegate::GetBlacklistURL() const {
  return GURL();
}

std::string SupervisedUserService::Delegate::GetSafeSitesCx() const {
  return std::string();
}

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
    const std::vector<scoped_refptr<SupervisedUserSiteList> >& site_lists) {
  ui_url_filter_->LoadWhitelists(site_lists);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SupervisedUserURLFilter::LoadWhitelists,
                                     io_url_filter_, site_lists));
}

void SupervisedUserService::URLFilterContext::LoadBlacklist(
    const base::FilePath& path,
    const base::Closure& callback) {
  // For now, support loading only once. If we want to support re-load, we'll
  // have to clear the blacklist pointer in the url filters first.
  DCHECK_EQ(0u, blacklist_.GetEntryCount());
  blacklist_.ReadFromFile(
      path,
      base::Bind(&SupervisedUserService::URLFilterContext::OnBlacklistLoaded,
                 base::Unretained(this), callback));
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

void SupervisedUserService::URLFilterContext::Clear() {
  ui_url_filter_->Clear();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::Clear,
                 io_url_filter_));
}

void SupervisedUserService::URLFilterContext::OnBlacklistLoaded(
    const base::Closure& callback) {
  ui_url_filter_->SetBlacklist(&blacklist_);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::SetBlacklist,
                 io_url_filter_,
                 &blacklist_));
  callback.Run();
}

void SupervisedUserService::URLFilterContext::InitAsyncURLChecker(
    const scoped_refptr<net::URLRequestContextGetter>& context,
    const std::string& cx) {
  ui_url_filter_->InitAsyncURLChecker(context.get(), cx);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SupervisedUserURLFilter::InitAsyncURLChecker,
                 io_url_filter_, context, cx));
}

SupervisedUserService::SupervisedUserService(Profile* profile)
    : includes_sync_sessions_type_(true),
      profile_(profile),
      active_(false),
      delegate_(NULL),
      waiting_for_sync_initialization_(false),
      is_profile_active_(false),
      did_init_(false),
      did_shutdown_(false),
      weak_ptr_factory_(this) {
  url_filter_context_.ui_url_filter()->AddObserver(this);
}

SupervisedUserService::~SupervisedUserService() {
  DCHECK(!did_init_ || did_shutdown_);
  url_filter_context_.ui_url_filter()->RemoveObserver(this);
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
  registry->RegisterBooleanPref(prefs::kSupervisedUserCreationAllowed, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  for (const char* pref : kCustodianInfoPrefs) {
    registry->RegisterStringPref(pref, std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
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

SupervisedUserWhitelistService* SupervisedUserService::GetWhitelistService() {
  return whitelist_service_.get();
}

std::string SupervisedUserService::GetCustodianEmailAddress() const {
  std::string custodian_email = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianEmail);
#if defined(OS_CHROMEOS)
  if (custodian_email.empty()) {
    custodian_email = chromeos::ChromeUserManager::Get()
        ->GetSupervisedUserManager()
        ->GetManagerDisplayEmail(
            user_manager::UserManager::Get()->GetActiveUser()->email());
  }
#endif
  return custodian_email;
}

std::string SupervisedUserService::GetCustodianName() const {
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianName);
#if defined(OS_CHROMEOS)
  if (name.empty()) {
    name = base::UTF16ToUTF8(chromeos::ChromeUserManager::Get()
        ->GetSupervisedUserManager()
        ->GetManagerDisplayName(
            user_manager::UserManager::Get()->GetActiveUser()->email()));
  }
#endif
  return name.empty() ? GetCustodianEmailAddress() : name;
}

std::string SupervisedUserService::GetSecondCustodianEmailAddress() const {
  return profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianEmail);
}

std::string SupervisedUserService::GetSecondCustodianName() const {
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianName);
  return name.empty() ? GetSecondCustodianEmailAddress() : name;
}

void SupervisedUserService::AddNavigationBlockedCallback(
    const NavigationBlockedCallback& callback) {
  navigation_blocked_callbacks_.push_back(callback);
}

void SupervisedUserService::DidBlockNavigation(
    content::WebContents* web_contents) {
  for (const auto& callback : navigation_blocked_callbacks_)
    callback.Run(web_contents);
}

void SupervisedUserService::AddObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SupervisedUserService::RemoveObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void SupervisedUserService::AddPermissionRequestCreator(
    scoped_ptr<PermissionRequestCreator> creator) {
  permissions_creators_.push_back(creator.release());
}

syncer::ModelTypeSet SupervisedUserService::GetPreferredDataTypes() const {
  if (!ProfileIsSupervised())
    return syncer::ModelTypeSet();

  syncer::ModelTypeSet result;
  if (IncludesSyncSessionsType())
    result.Put(syncer::SESSIONS);
  result.Put(syncer::EXTENSIONS);
  result.Put(syncer::EXTENSION_SETTINGS);
  result.Put(syncer::APPS);
  result.Put(syncer::APP_SETTINGS);
  result.Put(syncer::APP_NOTIFICATIONS);
  result.Put(syncer::APP_LIST);
  return result;
}

void SupervisedUserService::OnHistoryRecordingStateChanged() {
  bool record_history =
      profile_->GetPrefs()->GetBoolean(prefs::kRecordHistory);
  includes_sync_sessions_type_ = record_history;
  ProfileSyncServiceFactory::GetForProfile(profile_)
      ->ReconfigureDatatypeManager();
}

bool SupervisedUserService::IncludesSyncSessionsType() const {
  return includes_sync_sessions_type_;
}

void SupervisedUserService::OnStateChanged() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (waiting_for_sync_initialization_ && service->backend_initialized() &&
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
  if (service->backend_initialized() &&
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
  DCHECK(service->backend_initialized());
  DCHECK(service->backend_mode() == ProfileSyncService::SYNC);

  // Sync nothing (except types which are set via GetPreferredDataTypes).
  bool sync_everything = false;
  syncer::ModelTypeSet synced_datatypes;
  service->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  // Notify ProfileSyncService that we are done with configuration.
  service->SetSetupInProgress(false);
  service->SetSyncSetupCompleted();
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
  DCHECK(ProfileIsSupervised());
  ExtensionState result = GetExtensionState(extension);
  bool may_load = (result != EXTENSION_BLOCKED);
  if (!may_load && error)
    *error = l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER);
  return may_load;
}

// Note: Having MustRemainInstalled always say "true" for custodian-installed
// extensions does NOT prevent remote uninstalls (which is a bit unexpected, but
// exactly what we want).
bool SupervisedUserService::MustRemainInstalled(
    const extensions::Extension* extension,
    base::string16* error) const {
  DCHECK(ProfileIsSupervised());
  ExtensionState result = GetExtensionState(extension);
  bool may_not_uninstall = (result == EXTENSION_FORCED);
  if (may_not_uninstall && error)
    *error = l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER);
  return may_not_uninstall;
}

void SupervisedUserService::SetExtensionsActive() {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ManagementPolicy* management_policy =
      extension_system->management_policy();

  if (management_policy) {
    if (active_)
      management_policy->RegisterProvider(this);
    else
      management_policy->UnregisterProvider(this);
  }
}
#endif  // defined(ENABLE_EXTENSIONS)

SupervisedUserSettingsService* SupervisedUserService::GetSettingsService() {
  return SupervisedUserSettingsServiceFactory::GetForProfile(profile_);
}

size_t SupervisedUserService::FindEnabledPermissionRequestCreator(
    size_t start) {
  for (size_t i = start; i < permissions_creators_.size(); ++i) {
    if (permissions_creators_[i]->IsEnabled())
      return i;
  }
  return permissions_creators_.size();
}

void SupervisedUserService::AddPermissionRequestInternal(
    const CreatePermissionRequestCallback& create_request,
    const SuccessCallback& callback,
    size_t index) {
  // Find a permission request creator that is enabled.
  size_t next_index = FindEnabledPermissionRequestCreator(index);
  if (next_index >= permissions_creators_.size()) {
    callback.Run(false);
    return;
  }

  create_request.Run(
      permissions_creators_[next_index],
      base::Bind(&SupervisedUserService::OnPermissionRequestIssued,
                 weak_ptr_factory_.GetWeakPtr(), create_request,
                 callback, next_index));
}

void SupervisedUserService::OnPermissionRequestIssued(
    const CreatePermissionRequestCallback& create_request,
    const SuccessCallback& callback,
    size_t index,
    bool success) {
  if (success) {
    callback.Run(true);
    return;
  }

  AddPermissionRequestInternal(create_request, callback, index + 1);
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  SetActive(ProfileIsSupervised());
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

void SupervisedUserService::OnSiteListsChanged(
    const std::vector<scoped_refptr<SupervisedUserSiteList> >& site_lists) {
  url_filter_context_.LoadWhitelists(site_lists);
}

void SupervisedUserService::OnSiteListUpdated() {
  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
}

void SupervisedUserService::LoadBlacklist(const base::FilePath& path,
                                          const GURL& url) {
  if (!url.is_valid()) {
    LoadBlacklistFromFile(path);
    return;
  }

  DCHECK(!blacklist_downloader_);
  blacklist_downloader_.reset(new SupervisedUserBlacklistDownloader(
      url,
      path,
      profile_->GetRequestContext(),
      base::Bind(&SupervisedUserService::OnBlacklistDownloadDone,
                 base::Unretained(this), path)));
}

void SupervisedUserService::LoadBlacklistFromFile(const base::FilePath& path) {
  // This object is guaranteed to outlive the URLFilterContext, so we can bind a
  // raw pointer to it in the callback.
  url_filter_context_.LoadBlacklist(
      path, base::Bind(&SupervisedUserService::OnBlacklistLoaded,
                       base::Unretained(this)));
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

void SupervisedUserService::OnBlacklistLoaded() {
  FOR_EACH_OBSERVER(
      SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());
}

bool SupervisedUserService::AccessRequestsEnabled() {
  return FindEnabledPermissionRequestCreator(0) < permissions_creators_.size();
}

void SupervisedUserService::AddURLAccessRequest(
    const GURL& url,
    const SuccessCallback& callback) {
  AddPermissionRequestInternal(
      base::Bind(CreateURLAccessRequest,
                 SupervisedUserURLFilter::Normalize(url)),
      callback, 0);
}

void SupervisedUserService::AddExtensionUpdateRequest(
    const std::string& extension_id,
    const SuccessCallback& callback) {
  AddPermissionRequestInternal(
      base::Bind(CreateExtensionUpdateRequest, extension_id),
      callback, 0);
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
  pref_change_registrar_.Add(
      prefs::kRecordHistory,
      base::Bind(&SupervisedUserService::OnHistoryRecordingStateChanged,
                 base::Unretained(this)));

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Can be null in tests.
  if (sync_service)
    sync_service->AddPreferenceProvider(this);

  std::string client_id = component_updater::SupervisedUserWhitelistInstaller::
      ClientIdForProfilePath(profile_->GetPath());
  whitelist_service_.reset(new SupervisedUserWhitelistService(
      profile_->GetPrefs(),
      g_browser_process->supervised_user_whitelist_installer(), client_id));
  whitelist_service_->AddSiteListsChangedCallback(
      base::Bind(&SupervisedUserService::OnSiteListsChanged,
                 weak_ptr_factory_.GetWeakPtr()));

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

      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(switches::kSupervisedUserSyncToken)) {
        InitSync(
            command_line->GetSwitchValueASCII(
                switches::kSupervisedUserSyncToken));
      }

      ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
      token_service->LoadCredentials(
          supervised_users::kSupervisedUserPseudoEmail);

      permissions_creators_.push_back(new PermissionRequestCreatorSync(
          GetSettingsService(),
          SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
              profile_),
          ProfileSyncServiceFactory::GetForProfile(profile_),
          GetSupervisedUserName(),
          profile_->GetPrefs()->GetString(prefs::kSupervisedUserId)));

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

  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  sync_service->SetEncryptEverythingAllowed(!active_);

  GetSettingsService()->SetActive(active_);

#if defined(ENABLE_EXTENSIONS)
  SetExtensionsActive();
#endif

  if (active_) {
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
    for (const char* pref : kCustodianInfoPrefs) {
      pref_change_registrar_.Add(pref,
          base::Bind(&SupervisedUserService::OnCustodianInfoChanged,
                     base::Unretained(this)));
    }

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    whitelist_service_->Init();
    UpdateManualHosts();
    UpdateManualURLs();
    bool use_blacklist = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableSupervisedUserBlacklist);
    if (delegate_ && use_blacklist) {
      base::FilePath blacklist_path = delegate_->GetBlacklistPath();
      if (!blacklist_path.empty())
        LoadBlacklist(blacklist_path, delegate_->GetBlacklistURL());
    }
    bool use_safesites = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableSupervisedUserSafeSites);
    if (delegate_ && use_safesites) {
      const std::string& cx = delegate_->GetSafeSitesCx();
      if (!cx.empty()) {
        url_filter_context_.InitAsyncURLChecker(
            profile_->GetRequestContext(), cx);
      }
    }

#if !defined(OS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::AddObserver(this);
#endif
  } else {
    permissions_creators_.clear();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualHosts);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualURLs);
    for (const char* pref : kCustodianInfoPrefs) {
      pref_change_registrar_.Remove(pref);
    }

    url_filter_context_.Clear();
    FOR_EACH_OBSERVER(
        SupervisedUserServiceObserver, observer_list_, OnURLFilterChanged());

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
