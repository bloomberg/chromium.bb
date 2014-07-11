// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "webkit/browser/database/database_tracker.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#endif

using content::BrowserThread;
using content::DownloadManagerDelegate;
using content::HostZoomMap;

#if defined(ENABLE_EXTENSIONS)
namespace {

void NotifyOTRProfileCreatedOnIOThread(void* original_profile,
                                       void* otr_profile) {
  ExtensionWebRequestEventRouter::GetInstance()->OnOTRProfileCreated(
      original_profile, otr_profile);
}

void NotifyOTRProfileDestroyedOnIOThread(void* original_profile,
                                         void* otr_profile) {
  ExtensionWebRequestEventRouter::GetInstance()->OnOTRProfileDestroyed(
      original_profile, otr_profile);
}

}  // namespace
#endif

OffTheRecordProfileImpl::OffTheRecordProfileImpl(Profile* real_profile)
    : profile_(real_profile),
      prefs_(PrefServiceSyncable::IncognitoFromProfile(real_profile)),
      start_time_(Time::Now()) {
  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_);
}

void OffTheRecordProfileImpl::Init() {
  // The construction of OffTheRecordProfileIOData::Handle needs the profile
  // type returned by this->GetProfileType().  Since GetProfileType() is a
  // virtual member function, we cannot call the function defined in the most
  // derived class (e.g. GuestSessionProfile) until a ctor finishes.  Thus,
  // we have to instantiate OffTheRecordProfileIOData::Handle here after a ctor.
  InitIoData();

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
  // Because UserCloudPolicyManager is in a component, it cannot access
  // GetOriginalProfile. Instead, we have to inject this relation here.
  policy::UserCloudPolicyManagerFactory::RegisterForOffTheRecordBrowserContext(
      this->GetOriginalProfile(), this);
#endif

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  DCHECK_NE(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(profile_->GetPrefs()));

#if defined(OS_ANDROID) || defined(OS_IOS)
  UseSystemProxy();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  // TODO(oshima): Remove the need to eagerly initialize the request context
  // getter. chromeos::OnlineAttempt is illegally trying to access this
  // Profile member from a thread other than the UI thread, so we need to
  // prevent a race.
#if defined(OS_CHROMEOS)
  GetRequestContext();
#endif  // defined(OS_CHROMEOS)

  InitHostZoomMap();

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this).get(),
      io_data_->GetResourceContextNoInit());
#endif

#if defined(ENABLE_EXTENSIONS)
  // Make the chrome//extension-icon/ resource available.
  extensions::ExtensionIconSource* icon_source =
      new extensions::ExtensionIconSource(profile_);
  content::URLDataSource::Add(this, icon_source);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileCreatedOnIOThread, profile_, this));
#endif
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  MaybeSendDestroyedNotification();

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      io_data_->GetResourceContextNoInit());
#endif

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

#if defined(ENABLE_EXTENSIONS)
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileDestroyedOnIOThread, profile_, this));
#endif

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Clears any data the network stack contains that may be related to the
  // OTR session.
  g_browser_process->io_thread()->ChangedToOnTheRecord();
}

void OffTheRecordProfileImpl::InitIoData() {
  io_data_.reset(new OffTheRecordProfileIOData::Handle(this));
}

void OffTheRecordProfileImpl::InitHostZoomMap() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetForBrowserContext(profile_);
  host_zoom_map->CopyFrom(parent_host_zoom_map);
  // Observe parent's HZM change for propagating change of parent's
  // change to this HZM.
  zoom_subscription_ = parent_host_zoom_map->AddZoomLevelChangedCallback(
      base::Bind(&OffTheRecordProfileImpl::OnZoomLevelChanged,
                 base::Unretained(this)));
}

#if defined(OS_ANDROID) || defined(OS_IOS)
void OffTheRecordProfileImpl::UseSystemProxy() {
  // Force the use of the system-assigned proxy when off the record.
  const char kProxyMode[] = "mode";
  const char kProxyServer[] = "server";
  const char kProxyBypassList[] = "bypass_list";
  const char kProxyPacUrl[] = "pac_url";
  DictionaryPrefUpdate update(prefs_, prefs::kProxy);
  base::DictionaryValue* dict = update.Get();
  dict->SetString(kProxyMode, ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
  dict->SetString(kProxyPacUrl, "");
  dict->SetString(kProxyServer, "");
  dict->SetString(kProxyBypassList, "");
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

std::string OffTheRecordProfileImpl::GetProfileName() {
  // Incognito profile should not return the profile name.
  return std::string();
}

Profile::ProfileType OffTheRecordProfileImpl::GetProfileType() const {
  return INCOGNITO_PROFILE;
}

base::FilePath OffTheRecordProfileImpl::GetPath() const {
  return profile_->GetPath();
}

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

bool OffTheRecordProfileImpl::IsSupervised() {
  return GetOriginalProfile()->IsSupervised();
}

PrefService* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_;
}

PrefService* OffTheRecordProfileImpl::GetOffTheRecordPrefs() {
  return prefs_;
}

DownloadManagerDelegate* OffTheRecordProfileImpl::GetDownloadManagerDelegate() {
  return DownloadServiceFactory::GetForBrowserContext(this)->
      GetDownloadManagerDelegate();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_->CreateMainRequestContextGetter(
      protocol_handlers, request_interceptors.Pass()).get();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(
      renderer_child_id);
  return rph->GetStoragePartition()->GetURLRequestContext();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetMediaRequestContext() {
  // In OTR mode, media request context is the same as the original one.
  return GetRequestContext();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetMediaRequestContextForRenderProcess(
        int renderer_child_id) {
  // In OTR mode, media request context is the same as the original one.
  return GetRequestContextForRenderProcess(renderer_child_id);
}

net::URLRequestContextGetter*
OffTheRecordProfileImpl::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return io_data_->GetIsolatedAppRequestContextGetter(partition_path, in_memory)
      .get();
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
      partition_path,
      in_memory,
      protocol_handlers,
      request_interceptors.Pass()).get();
}

content::ResourceContext* OffTheRecordProfileImpl::GetResourceContext() {
  return io_data_->GetResourceContext();
}

net::SSLConfigService* OffTheRecordProfileImpl::GetSSLConfigService() {
  return profile_->GetSSLConfigService();
}

HostContentSettingsMap* OffTheRecordProfileImpl::GetHostContentSettingsMap() {
  // Retrieve the host content settings map of the parent profile in order to
  // ensure the preferences have been migrated.
  profile_->GetHostContentSettingsMap();
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), true);
#if defined(ENABLE_EXTENSIONS)
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(this)->extension_service();
    if (extension_service)
      host_content_settings_map_->RegisterExtensionService(extension_service);
#endif
  }
  return host_content_settings_map_.get();
}

content::BrowserPluginGuestManager*
    OffTheRecordProfileImpl::GetGuestManager() {
#if defined(ENABLE_EXTENSIONS)
  return GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

quota::SpecialStoragePolicy*
    OffTheRecordProfileImpl::GetSpecialStoragePolicy() {
  return GetExtensionSpecialStoragePolicy();
}

content::PushMessagingService*
OffTheRecordProfileImpl::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in incognito if possible.
  return NULL;
}

bool OffTheRecordProfileImpl::IsSameProfile(Profile* profile) {
  return (profile == this) || (profile == profile_);
}

Time OffTheRecordProfileImpl::GetStartTime() const {
  return start_time_;
}

history::TopSites* OffTheRecordProfileImpl::GetTopSitesWithoutCreating() {
  return NULL;
}

history::TopSites* OffTheRecordProfileImpl::GetTopSites() {
  return NULL;
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

DevToolsNetworkController*
OffTheRecordProfileImpl::GetDevToolsNetworkController() {
  return io_data_->GetDevToolsNetworkController();
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
  }

  virtual ProfileType GetProfileType() const OVERRIDE {
    return GUEST_PROFILE;
  }

  virtual void InitChromeOSPreferences() OVERRIDE {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(static_cast<PrefServiceSyncable*>(GetPrefs()),
                                chromeos::UserManager::Get()->GetActiveUser());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;
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

void OffTheRecordProfileImpl::OnZoomLevelChanged(
    const HostZoomMap::ZoomLevelChange& change) {
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
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
  }
}

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
