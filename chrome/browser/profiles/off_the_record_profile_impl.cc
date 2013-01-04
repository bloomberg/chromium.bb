// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_factory.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/transport_security_state.h"
#include "net/http/http_server_properties.h"
#include "webkit/database/database_tracker.h"

#if defined(OS_ANDROID)
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif

using content::BrowserThread;
using content::DownloadManagerDelegate;
using content::HostZoomMap;

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

OffTheRecordProfileImpl::OffTheRecordProfileImpl(Profile* real_profile)
    : profile_(real_profile),
      prefs_(real_profile->GetOffTheRecordPrefs()),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
      start_time_(Time::Now()) {
}

void OffTheRecordProfileImpl::Init() {
  ProfileDependencyManager::GetInstance()->CreateProfileServices(this, false);

  extensions::ExtensionSystem::Get(this)->InitForOTRProfile();

  DCHECK_NE(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(profile_->GetPrefs()));

#if defined(OS_ANDROID)
  UseSystemProxy();
#endif  // defined(OS_ANDROID)

  // TODO(oshima): Remove the need to eagerly initialize the request context
  // getter. chromeos::OnlineAttempt is illegally trying to access this
  // Profile member from a thread other than the UI thread, so we need to
  // prevent a race.
#if defined(OS_CHROMEOS)
  GetRequestContext();
#endif  // defined(OS_CHROMEOS)

  InitHostZoomMap();

  // Make the chrome//extension-icon/ resource available.
  ExtensionIconSource* icon_source = new ExtensionIconSource(profile_);
  ChromeURLDataManager::AddDataSource(this, icon_source);

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this), io_data_.GetResourceContextNoInit());
#endif

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileCreatedOnIOThread, profile_, this));
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  MaybeSendDestroyedNotification();

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
    io_data_.GetResourceContextNoInit());
#endif

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(this)->extension_service();
  if (extension_service && extension_service->extensions_enabled()) {
    extension_service->extension_prefs()->
        ClearIncognitoSessionOnlyContentSettings();
  }

  ProfileDependencyManager::GetInstance()->DestroyProfileServices(this);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileDestroyedOnIOThread, profile_, this));

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Clears any data the network stack contains that may be related to the
  // OTR session.
  g_browser_process->io_thread()->ChangedToOnTheRecord();
}

void OffTheRecordProfileImpl::InitHostZoomMap() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetForBrowserContext(profile_);
  host_zoom_map->CopyFrom(parent_host_zoom_map);
  // Observe parent's HZM change for propagating change of parent's
  // change to this HZM.
  registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                 content::Source<HostZoomMap>(parent_host_zoom_map));
}

#if defined(OS_ANDROID)
void OffTheRecordProfileImpl::UseSystemProxy() {
  // Force the use of the system-assigned proxy when off the record.
  const char kProxyMode[] = "mode";
  const char kProxyServer[] = "server";
  const char kProxyBypassList[] = "bypass_list";
  const char kProxyPacUrl[] = "pac_url";
  DictionaryPrefUpdate update(prefs_, prefs::kProxy);
  DictionaryValue* dict = update.Get();
  dict->SetString(kProxyMode, ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
  dict->SetString(kProxyPacUrl, "");
  dict->SetString(kProxyServer, "");
  dict->SetString(kProxyBypassList, "");
}
#endif  // defined(OS_ANDROID)

std::string OffTheRecordProfileImpl::GetProfileName() {
  // Incognito profile should not return the profile name.
  return std::string();
}

FilePath OffTheRecordProfileImpl::GetPath() {
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

ExtensionService* OffTheRecordProfileImpl::GetExtensionService() {
  return extensions::ExtensionSystem::Get(this)->extension_service();
}

ExtensionSpecialStoragePolicy*
    OffTheRecordProfileImpl::GetExtensionSpecialStoragePolicy() {
  return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
}

policy::ManagedModePolicyProvider*
    OffTheRecordProfileImpl::GetManagedModePolicyProvider() {
  return profile_->GetManagedModePolicyProvider();
}

policy::PolicyService* OffTheRecordProfileImpl::GetPolicyService() {
  return profile_->GetPolicyService();
}

PrefServiceSyncable* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_;
}

PrefServiceSyncable* OffTheRecordProfileImpl::GetOffTheRecordPrefs() {
  return prefs_;
}

DownloadManagerDelegate* OffTheRecordProfileImpl::GetDownloadManagerDelegate() {
  return DownloadServiceFactory::GetForProfile(this)->
      GetDownloadManagerDelegate();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::GetRequestContext() {
  return io_data_.GetMainRequestContextGetter();
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
    const FilePath& partition_path,
    bool in_memory) {
  return GetRequestContextForStoragePartition(partition_path, in_memory);
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForStoragePartition(
        const FilePath& partition_path,
        bool in_memory) {
  return io_data_.GetIsolatedAppRequestContextGetter(partition_path, in_memory);
}

content::ResourceContext* OffTheRecordProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
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
    ExtensionService* extension_service = GetExtensionService();
    if (extension_service)
      host_content_settings_map_->RegisterExtensionService(extension_service);
#endif
  }
  return host_content_settings_map_.get();
}

content::GeolocationPermissionContext*
    OffTheRecordProfileImpl::GetGeolocationPermissionContext() {
  return profile_->GetGeolocationPermissionContext();
}

content::SpeechRecognitionPreferences*
    OffTheRecordProfileImpl::GetSpeechRecognitionPreferences() {
  return profile_->GetSpeechRecognitionPreferences();
}

quota::SpecialStoragePolicy*
    OffTheRecordProfileImpl::GetSpecialStoragePolicy() {
  return GetExtensionSpecialStoragePolicy();
}

ProtocolHandlerRegistry* OffTheRecordProfileImpl::GetProtocolHandlerRegistry() {
  return profile_->GetProtocolHandlerRegistry();
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

FilePath OffTheRecordProfileImpl::last_selected_directory() {
  const FilePath& directory = last_selected_directory_;
  if (directory.empty()) {
    return profile_->last_selected_directory();
  }
  return directory;
}

void OffTheRecordProfileImpl::set_last_selected_directory(
    const FilePath& path) {
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
void OffTheRecordProfileImpl::SetupChromeOSEnterpriseExtensionObserver() {
  profile_->SetupChromeOSEnterpriseExtensionObserver();
}

void OffTheRecordProfileImpl::InitChromeOSPreferences() {
  // The incognito profile shouldn't have Chrome OS's preferences.
  // The preferences are associated with the regular user profile.
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
void OffTheRecordProfileImpl::ChangeAppLocale(const std::string& locale,
                                              AppLocaleChangedVia) {
}

void OffTheRecordProfileImpl::OnLogin() {
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* OffTheRecordProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_.get()) {
    pref_proxy_config_tracker_.reset(
        ProxyServiceFactory::CreatePrefProxyConfigTracker(GetPrefs()));
  }
  return pref_proxy_config_tracker_.get();
}

chrome_browser_net::Predictor* OffTheRecordProfileImpl::GetNetworkPredictor() {
  // We do not store information about websites visited in OTR profiles which
  // is necessary for a Predictor, so we do not have a Predictor at all.
  return NULL;
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

void OffTheRecordProfileImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_ZOOM_LEVEL_CHANGED) {
    const std::string& host =
        *(content::Details<const std::string>(details).ptr());
    if (!host.empty()) {
      HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
      HostZoomMap* parent_host_zoom_map =
          HostZoomMap::GetForBrowserContext(profile_);
      double level = parent_host_zoom_map->GetZoomLevel(host);
      host_zoom_map->SetZoomLevel(host, level);
    }
  }
}

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile) {
  }

  virtual void InitChromeOSPreferences() {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(GetPrefs());
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

base::Callback<ChromeURLDataManagerBackend*(void)>
    OffTheRecordProfileImpl::GetChromeURLDataManagerBackendGetter() const {
  return io_data_.GetChromeURLDataManagerBackendGetter();
}
