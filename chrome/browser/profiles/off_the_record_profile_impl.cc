// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_url_request_context_getter.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/json_pref_store.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/features/features.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "ppapi/features/features.h"
#include "services/preferences/public/cpp/pref_service_main.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "storage/browser/database/database_tracker.h"

#if defined(OS_ANDROID)
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/proxy_prefs.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_otr_delegate.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/host_zoom_map.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using content::BrowserThread;
using content::DownloadManagerDelegate;
#if !defined(OS_ANDROID)
using content::HostZoomMap;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace {

void NotifyOTRProfileCreatedOnIOThread(void* original_profile,
                                       void* otr_profile) {
  extensions::ExtensionWebRequestEventRouter::GetInstance()
      ->OnOTRBrowserContextCreated(original_profile, otr_profile);
}

void NotifyOTRProfileDestroyedOnIOThread(void* original_profile,
                                         void* otr_profile) {
  extensions::ExtensionWebRequestEventRouter::GetInstance()
      ->OnOTRBrowserContextDestroyed(original_profile, otr_profile);
}

}  // namespace
#endif

PrefStore* CreateExtensionPrefStore(Profile* profile,
                                    bool incognito_pref_store) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return new ExtensionPrefStore(
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile),
      incognito_pref_store);
#else
  return NULL;
#endif
}

OffTheRecordProfileImpl::OffTheRecordProfileImpl(Profile* real_profile)
    : profile_(real_profile),
      start_time_(Time::Now()) {
  // Must happen before we ask for prefs as prefs needs the connection to the
  // service manager, which is set up in Initialize.
  BrowserContext::Initialize(this, profile_->GetPath());
  service_manager::Connector* otr_connector = nullptr;
  service_manager::Connector* user_connector = nullptr;
  std::set<PrefValueStore::PrefStoreType> already_connected_types;
  if (features::PrefServiceEnabled()) {
    otr_connector = content::BrowserContext::GetConnectorFor(this);
    user_connector = content::BrowserContext::GetConnectorFor(profile_);
    already_connected_types = chrome::InProcessPrefStores();
  }
  prefs_.reset(CreateIncognitoPrefServiceSyncable(
      PrefServiceSyncableFromProfile(profile_),
      CreateExtensionPrefStore(profile_, true),
      std::move(already_connected_types), otr_connector, user_connector));
  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

void OffTheRecordProfileImpl::Init() {
  // The construction of OffTheRecordProfileIOData::Handle needs the profile
  // type returned by this->GetProfileType().  Since GetProfileType() is a
  // virtual member function, we cannot call the function defined in the most
  // derived class (e.g. GuestSessionProfile) until a ctor finishes.  Thus,
  // we have to instantiate OffTheRecordProfileIOData::Handle here after a ctor.
  InitIoData();

#if !defined(OS_CHROMEOS)
  // Because UserCloudPolicyManager is in a component, it cannot access
  // GetOriginalProfile. Instead, we have to inject this relation here.
  policy::UserCloudPolicyManagerFactory::RegisterForOffTheRecordBrowserContext(
      this->GetOriginalProfile(), this);
#endif

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  set_is_guest_profile(
      profile_->GetPath() == ProfileManager::GetGuestProfilePath());

  // Guest profiles may always be OTR. Check IncognitoModePrefs otherwise.
  DCHECK(profile_->IsGuestSession() ||
         IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
             IncognitoModePrefs::DISABLED);

#if !defined(OS_ANDROID)
  TrackZoomLevelsFromParent();
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      this, io_data_->GetResourceContextNoInit());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Make the chrome//extension-icon/ resource available.
  extensions::ExtensionIconSource* icon_source =
      new extensions::ExtensionIconSource(profile_);
  content::URLDataSource::Add(this, icon_source);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&NotifyOTRProfileCreatedOnIOThread, profile_, this));
#endif

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  dom_distiller::RegisterViewerSource(this);
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  MaybeSendDestroyedNotification();

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      io_data_->GetResourceContextNoInit());
#endif

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&NotifyOTRProfileDestroyedOnIOThread, profile_, this));
#endif

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Clears any data the network stack contains that may be related to the
  // OTR session.
  g_browser_process->io_thread()->ChangedToOnTheRecord();

  // This must be called before ProfileIOData::ShutdownOnUIThread but after
  // other profile-related destroy notifications are dispatched.
  ShutdownStoragePartitions();
}

void OffTheRecordProfileImpl::InitIoData() {
  io_data_.reset(new OffTheRecordProfileIOData::Handle(this));
}

#if !defined(OS_ANDROID)
void OffTheRecordProfileImpl::TrackZoomLevelsFromParent() {
  DCHECK_NE(INCOGNITO_PROFILE, profile_->GetProfileType());

  // Here we only want to use zoom levels stored in the main-context's default
  // storage partition. We're not interested in zoom levels in special
  // partitions, e.g. those used by WebViewGuests.
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(profile_);
  host_zoom_map->CopyFrom(parent_host_zoom_map);
  // Observe parent profile's HostZoomMap changes so they can also be applied
  // to this profile's HostZoomMap.
  track_zoom_subscription_ = parent_host_zoom_map->AddZoomLevelChangedCallback(
      base::Bind(&OffTheRecordProfileImpl::OnParentZoomLevelChanged,
                 base::Unretained(this)));
  if (!profile_->GetZoomLevelPrefs())
    return;

  // Also track changes to the parent profile's default zoom level.
  parent_default_zoom_level_subscription_ =
      profile_->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          base::Bind(&OffTheRecordProfileImpl::UpdateDefaultZoomLevel,
                     base::Unretained(this)));
}
#endif  // !defined(OS_ANDROID)

std::string OffTheRecordProfileImpl::GetProfileUserName() const {
  // Incognito profile should not return the username.
  return std::string();
}

Profile::ProfileType OffTheRecordProfileImpl::GetProfileType() const {
#if !defined(OS_CHROMEOS)
  return profile_->IsGuestSession() ? GUEST_PROFILE : INCOGNITO_PROFILE;
#else
  return INCOGNITO_PROFILE;
#endif
}

base::FilePath OffTheRecordProfileImpl::GetPath() const {
  return profile_->GetPath();
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
OffTheRecordProfileImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return base::MakeUnique<ChromeZoomLevelOTRDelegate>(
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}
#endif  // !defined(OS_ANDROID)

scoped_refptr<base::SequencedTaskRunner>
OffTheRecordProfileImpl::GetIOTaskRunner() {
  return profile_->GetIOTaskRunner();
}

bool OffTheRecordProfileImpl::IsOffTheRecord() const {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOffTheRecordProfile() {
  return this;
}

void OffTheRecordProfileImpl::DestroyOffTheRecordProfile() {
  // Suicide is bad!
  NOTREACHED();
}

bool OffTheRecordProfileImpl::HasOffTheRecordProfile() {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOriginalProfile() {
  return profile_;
}

ExtensionSpecialStoragePolicy*
    OffTheRecordProfileImpl::GetExtensionSpecialStoragePolicy() {
  return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
}

bool OffTheRecordProfileImpl::IsSupervised() const {
  return profile_->IsSupervised();
}

bool OffTheRecordProfileImpl::IsChild() const {
  // TODO(treib): If we ever allow incognito for child accounts, evaluate
  // whether we want to just return false here.
  return profile_->IsChild();
}

bool OffTheRecordProfileImpl::IsLegacySupervised() const {
  return profile_->IsLegacySupervised();
}

PrefService* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_.get();
}

const PrefService* OffTheRecordProfileImpl::GetPrefs() const {
  return prefs_.get();
}

PrefService* OffTheRecordProfileImpl::GetOffTheRecordPrefs() {
  return prefs_.get();
}

DownloadManagerDelegate* OffTheRecordProfileImpl::GetDownloadManagerDelegate() {
  return DownloadCoreServiceFactory::GetForBrowserContext(this)
      ->GetDownloadManagerDelegate();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_->CreateMainRequestContextGetter(
                     protocol_handlers, std::move(request_interceptors))
      .get();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::CreateMediaRequestContext() {
  // In OTR mode, media request context is the same as the original one.
  return GetRequestContext();
}

net::URLRequestContextGetter*
OffTheRecordProfileImpl::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return io_data_->GetIsolatedAppRequestContextGetter(partition_path, in_memory)
      .get();
}

void OffTheRecordProfileImpl::RegisterInProcessServices(
    StaticServiceMap* services) {
  if (features::PrefServiceEnabled()) {
    content::ServiceInfo info;
    info.factory = base::Bind(
        &prefs::CreatePrefService, chrome::ExpectedPrefStores(),
        make_scoped_refptr(content::BrowserThread::GetBlockingPool()));
    info.task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
         base::TaskPriority::USER_VISIBLE});
    pref_service_task_runner_ = info.task_runner;
    services->insert(std::make_pair(prefs::mojom::kServiceName, info));
  }
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForExtensions() {
  return io_data_->GetExtensionsRequestContextGetter().get();
}

net::URLRequestContextGetter*
OffTheRecordProfileImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_->CreateIsolatedAppRequestContextGetter(
                     partition_path, in_memory, protocol_handlers,
                     std::move(request_interceptors))
      .get();
}

content::ResourceContext* OffTheRecordProfileImpl::GetResourceContext() {
  return io_data_->GetResourceContext();
}

net::SSLConfigService* OffTheRecordProfileImpl::GetSSLConfigService() {
  return profile_->GetSSLConfigService();
}

content::BrowserPluginGuestManager* OffTheRecordProfileImpl::GetGuestManager() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

storage::SpecialStoragePolicy*
OffTheRecordProfileImpl::GetSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PushMessagingService*
OffTheRecordProfileImpl::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in incognito if possible.
  return NULL;
}

content::SSLHostStateDelegate*
OffTheRecordProfileImpl::GetSSLHostStateDelegate() {
  return ChromeSSLHostStateDelegateFactory::GetForProfile(this);
}

// TODO(mlamouri): we should all these BrowserContext implementation to Profile
// instead of repeating them inside all Profile implementations.
content::PermissionManager* OffTheRecordProfileImpl::GetPermissionManager() {
  return PermissionManagerFactory::GetForProfile(this);
}

content::BackgroundSyncController*
OffTheRecordProfileImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForProfile(this);
}

content::BrowsingDataRemoverDelegate*
OffTheRecordProfileImpl::GetBrowsingDataRemoverDelegate() {
  return ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(this);
}

bool OffTheRecordProfileImpl::IsSameProfile(Profile* profile) {
  return (profile == this) || (profile == profile_);
}

Time OffTheRecordProfileImpl::GetStartTime() const {
  return start_time_;
}

void OffTheRecordProfileImpl::SetExitType(ExitType exit_type) {
}

base::FilePath OffTheRecordProfileImpl::last_selected_directory() {
  const base::FilePath& directory = last_selected_directory_;
  if (directory.empty()) {
    return profile_->last_selected_directory();
  }
  return directory;
}

void OffTheRecordProfileImpl::set_last_selected_directory(
    const base::FilePath& path) {
  last_selected_directory_ = path;
}

bool OffTheRecordProfileImpl::WasCreatedByVersionOrLater(
    const std::string& version) {
  return profile_->WasCreatedByVersionOrLater(version);
}

Profile::ExitType OffTheRecordProfileImpl::GetLastSessionExitType() {
  return profile_->GetLastSessionExitType();
}

scoped_refptr<base::SequencedTaskRunner>
OffTheRecordProfileImpl::GetPrefServiceTaskRunner() {
  return pref_service_task_runner_;
}

#if defined(OS_CHROMEOS)
void OffTheRecordProfileImpl::ChangeAppLocale(const std::string& locale,
                                              AppLocaleChangedVia) {
}

void OffTheRecordProfileImpl::OnLogin() {
}

void OffTheRecordProfileImpl::InitChromeOSPreferences() {
  // The incognito profile shouldn't have Chrome OS's preferences.
  // The preferences are associated with the regular user profile.
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* OffTheRecordProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_)
    pref_proxy_config_tracker_.reset(CreateProxyConfigTracker());
  return pref_proxy_config_tracker_.get();
}

chrome_browser_net::Predictor* OffTheRecordProfileImpl::GetNetworkPredictor() {
  // We do not store information about websites visited in OTR profiles which
  // is necessary for a Predictor, so we do not have a Predictor at all.
  return NULL;
}

DevToolsNetworkControllerHandle*
OffTheRecordProfileImpl::GetDevToolsNetworkControllerHandle() {
  return io_data_->GetDevToolsNetworkControllerHandle();
}

void OffTheRecordProfileImpl::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  // Nothing to do here, our transport security state is read-only.
  // Still, fire the callback to indicate we have finished, otherwise the
  // BrowsingDataRemover will never be destroyed and the dialog will never be
  // closed. We must do this asynchronously in order to avoid reentrancy issues.
  if (!completion.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion);
  }
}

GURL OffTheRecordProfileImpl::GetHomePage() {
  return profile_->GetHomePage();
}

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile) {
    set_is_guest_profile(true);
  }

  ProfileType GetProfileType() const override { return GUEST_PROFILE; }

  void InitChromeOSPreferences() override {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(
        this, user_manager::UserManager::Get()->GetActiveUser());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  std::unique_ptr<chromeos::Preferences> chromeos_preferences_;
};
#endif

Profile* Profile::CreateOffTheRecordProfile() {
  OffTheRecordProfileImpl* profile = NULL;
#if defined(OS_CHROMEOS)
  if (IsGuestSession())
    profile = new GuestSessionProfile(this);
#endif
  if (!profile)
    profile = new OffTheRecordProfileImpl(this);
  profile->Init();
  return profile;
}

#if !defined(OS_ANDROID)
void OffTheRecordProfileImpl::OnParentZoomLevelChanged(
    const HostZoomMap::ZoomLevelChange& change) {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  switch (change.mode) {
    case HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_HOST:
      host_zoom_map->SetZoomLevelForHost(change.host, change.zoom_level);
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
      host_zoom_map->SetZoomLevelForHostAndScheme(change.scheme,
          change.host,
          change.zoom_level);
      return;
    case HostZoomMap::PAGE_SCALE_IS_ONE_CHANGED:
      return;
  }
}

void OffTheRecordProfileImpl::UpdateDefaultZoomLevel() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  double default_zoom_level =
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  host_zoom_map->SetDefaultZoomLevel(default_zoom_level);
  // HostZoomMap does not trigger zoom notification events when the default
  // zoom level is set, so we need to do it here.
  zoom::ZoomEventManager::GetForBrowserContext(this)
      ->OnDefaultZoomLevelChanged();
}
#endif  // !defined(OS_ANDROID)

PrefProxyConfigTracker* OffTheRecordProfileImpl::CreateProxyConfigTracker() {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this)) {
    return ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
        g_browser_process->local_state());
  }
#endif  // defined(OS_CHROMEOS)
  return ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
      GetPrefs(), g_browser_process->local_state());
}
