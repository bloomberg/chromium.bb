// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include <utility>

#include "android_webview/browser/aw_browser_policy_connector.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_metrics_service_client.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/aw_pref_store.h"
#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/common/aw_content_client.h"
#include "base/base_paths_android.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/cookie_store.h"
#include "net/proxy/proxy_config_service_android.h"
#include "net/proxy/proxy_service.h"

using base::FilePath;
using content::BrowserThread;

namespace android_webview {

namespace prefs {

// String that specifies the Android account type to use for Negotiate
// authentication.
const char kAuthAndroidNegotiateAccountType[] =
    "auth.android_negotiate_account_type";

// Whitelist containing servers for which Integrated Authentication is enabled.
const char kAuthServerWhitelist[] = "auth.server_whitelist";

}  // namespace prefs

namespace {
// Name of the preference that governs enabling the Data Reduction Proxy.
const char kDataReductionProxyEnabled[] = "data_reduction_proxy.enabled";

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

void DeleteDirRecursively(const base::FilePath& path) {
  if (!base::DeleteFile(path, true)) {
    // Deleting a non-existent file is considered successful, so this will
    // trigger only in case of real errors.
    LOG(WARNING) << "Failed to delete " << path.AsUTF8Unsafe();
  }
}

AwBrowserContext* g_browser_context = NULL;

scoped_ptr<net::ProxyConfigService> CreateProxyConfigService() {
  scoped_ptr<net::ProxyConfigService> config_service =
      net::ProxyService::CreateSystemProxyConfigService(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          nullptr /* Ignored on Android */);

  // TODO(csharrison) Architect the wrapper better so we don't need a cast for
  // android ProxyConfigServices.
  net::ProxyConfigServiceAndroid* android_config_service =
      static_cast<net::ProxyConfigServiceAndroid*>(config_service.get());
  android_config_service->set_exclude_pac_url(true);
  return config_service;
}

bool OverrideBlacklistForURL(const GURL& url, bool* block, int* reason) {
  // We don't have URLs that should never be blacklisted here.
  return false;
}

policy::URLBlacklistManager* CreateURLBlackListManager(
    PrefService* pref_service) {
  policy::URLBlacklist::SegmentURLCallback segment_url_callback =
      static_cast<policy::URLBlacklist::SegmentURLCallback>(
          url_formatter::SegmentURL);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      pool->GetSequencedTaskRunner(pool->GetSequenceToken());
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);

  return new policy::URLBlacklistManager(pref_service, background_task_runner,
                                         io_task_runner, segment_url_callback,
                                         base::Bind(OverrideBlacklistForURL));
}

}  // namespace

// Data reduction proxy is disabled by default.
bool AwBrowserContext::data_reduction_proxy_enabled_ = false;

// Delete the legacy cache dir (in the app data dir) in 10 seconds after init.
int AwBrowserContext::legacy_cache_removal_delay_ms_ = 10000;

AwBrowserContext::AwBrowserContext(
    const FilePath path,
    JniDependencyFactory* native_factory)
    : context_storage_path_(path),
      native_factory_(native_factory) {
  DCHECK(!g_browser_context);
  g_browser_context = this;

  // This constructor is entered during the creation of ContentBrowserClient,
  // before browser threads are created. Therefore any checks to enforce
  // threading (such as BrowserThread::CurrentlyOn()) will fail here.
}

AwBrowserContext::~AwBrowserContext() {
  DCHECK_EQ(this, g_browser_context);
  g_browser_context = NULL;
}

// static
AwBrowserContext* AwBrowserContext::GetDefault() {
  // TODO(joth): rather than store in a global here, lookup this instance
  // from the Java-side peer.
  return g_browser_context;
}

// static
AwBrowserContext* AwBrowserContext::FromWebContents(
    content::WebContents* web_contents) {
  // This is safe; this is the only implementation of the browser context.
  return static_cast<AwBrowserContext*>(web_contents->GetBrowserContext());
}

// static
void AwBrowserContext::SetDataReductionProxyEnabled(bool enabled) {
  // Cache the setting value. It is possible that data reduction proxy is
  // not created yet.
  data_reduction_proxy_enabled_ = enabled;
  AwBrowserContext* context = AwBrowserContext::GetDefault();
  // Can't enable Data reduction proxy if user pref service is not ready.
  if (context == NULL || context->user_pref_service_.get() == NULL)
    return;
  data_reduction_proxy::DataReductionProxySettings* proxy_settings =
      context->GetDataReductionProxySettings();
  if (proxy_settings == NULL)
    return;
  // At this point, context->PreMainMessageLoopRun() has run, so
  // context->data_reduction_proxy_io_data() is valid.
  DCHECK(context->GetDataReductionProxyIOData());
  context->CreateDataReductionProxyStatisticsIfNecessary();
  proxy_settings->SetDataReductionProxyEnabled(data_reduction_proxy_enabled_);
}

// static
void AwBrowserContext::SetLegacyCacheRemovalDelayForTest(int delay_ms) {
  legacy_cache_removal_delay_ms_ = delay_ms;
}

void AwBrowserContext::PreMainMessageLoopRun() {
  cookie_store_ = CreateCookieStore(this);
  FilePath cache_path;
  const FilePath fallback_cache_dir =
      GetPath().Append(FILE_PATH_LITERAL("Cache"));
  if (PathService::Get(base::DIR_CACHE, &cache_path)) {
    cache_path = cache_path.Append(
        FILE_PATH_LITERAL("org.chromium.android_webview"));
    // Delay the legacy dir removal to not impact startup performance.
    BrowserThread::PostDelayedTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DeleteDirRecursively, fallback_cache_dir),
        base::TimeDelta::FromMilliseconds(legacy_cache_removal_delay_ms_));
  } else {
    cache_path = fallback_cache_dir;
    LOG(WARNING) << "Failed to get cache directory for Android WebView. "
                 << "Using app data directory as a fallback.";
  }

  browser_policy_connector_.reset(new AwBrowserPolicyConnector());

  InitUserPrefService();

  url_request_context_getter_ = new AwURLRequestContextGetter(
      cache_path, cookie_store_.get(), CreateProxyConfigService(),
      user_pref_service_.get());

  data_reduction_proxy_io_data_.reset(
      new data_reduction_proxy::DataReductionProxyIOData(
          data_reduction_proxy::Client::WEBVIEW_ANDROID,
          data_reduction_proxy::DataReductionProxyParams::kAllowed,
          url_request_context_getter_->GetNetLog(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          false /* enable */,
          false /* enable_quic */,
          GetUserAgent()));
  data_reduction_proxy_settings_.reset(
      new data_reduction_proxy::DataReductionProxySettings());
  scoped_ptr<data_reduction_proxy::DataStore> store(
      new data_reduction_proxy::DataStore());
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      pool->GetSequencedTaskRunnerWithShutdownBehavior(
          pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  data_reduction_proxy_service_.reset(
      new data_reduction_proxy::DataReductionProxyService(
          data_reduction_proxy_settings_.get(), nullptr,
          GetAwURLRequestContext(), std::move(store),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          db_task_runner, base::TimeDelta()));
  data_reduction_proxy_io_data_->SetDataReductionProxyService(
      data_reduction_proxy_service_->GetWeakPtr());

  visitedlink_master_.reset(
      new visitedlink::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();

  form_database_service_.reset(
      new AwFormDatabaseService(context_storage_path_));

  // Ensure the storage partition is initialized in time for DataReductionProxy.
  EnsureResourceContextInitialized(this);

  // TODO(dgn) lazy init, see http://crbug.com/521542
  data_reduction_proxy_settings_->InitDataReductionProxySettings(
      kDataReductionProxyEnabled, user_pref_service_.get(),
      data_reduction_proxy_io_data_.get(),
      std::move(data_reduction_proxy_service_));
  data_reduction_proxy_settings_->MaybeActivateDataReductionProxy(true);

  blacklist_manager_.reset(CreateURLBlackListManager(user_pref_service_.get()));

  // UMA uses randomly-generated GUIDs (globally unique identifiers) to
  // anonymously identify logs. Every WebView-using app on every device
  // is given a GUID, stored in this file in the app's data directory.
  const FilePath guid_file_path =
      GetPath().Append(FILE_PATH_LITERAL("metrics_guid"));

  AwMetricsServiceClient::GetInstance()->Initialize(user_pref_service_.get(),
                                                    GetRequestContext(),
                                                    guid_file_path);
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

net::URLRequestContextGetter* AwBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  // This function cannot actually create the request context because
  // there is a reentrant dependency on GetResourceContext() via
  // content::StoragePartitionImplMap::Create(). This is not fixable
  // until http://crbug.com/159193. Until then, assert that the context
  // has already been allocated and just handle setting the protocol_handlers.
  DCHECK(url_request_context_getter_.get());
  url_request_context_getter_->SetHandlersAndInterceptors(
      protocol_handlers, std::move(request_interceptors));
  return url_request_context_getter_.get();
}

net::URLRequestContextGetter*
AwBrowserContext::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  NOTREACHED();
  return NULL;
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_.get()) {
    quota_manager_bridge_ = native_factory_->CreateAwQuotaManagerBridge(this);
  }
  return quota_manager_bridge_.get();
}

AwFormDatabaseService* AwBrowserContext::GetFormDatabaseService() {
  return form_database_service_.get();
}

data_reduction_proxy::DataReductionProxySettings*
AwBrowserContext::GetDataReductionProxySettings() {
  return data_reduction_proxy_settings_.get();
}

data_reduction_proxy::DataReductionProxyIOData*
AwBrowserContext::GetDataReductionProxyIOData() {
  return data_reduction_proxy_io_data_.get();
}

AwURLRequestContextGetter* AwBrowserContext::GetAwURLRequestContext() {
  return url_request_context_getter_.get();
}

AwMessagePortService* AwBrowserContext::GetMessagePortService() {
  if (!message_port_service_.get()) {
    message_port_service_.reset(
        native_factory_->CreateAwMessagePortService());
  }
  return message_port_service_.get();
}

// Create user pref service
void AwBrowserContext::InitUserPrefService() {
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable();
  // We only use the autocomplete feature of Autofill, which is controlled via
  // the manager_delegate. We don't use the rest of Autofill, which is why it is
  // hardcoded as disabled here.
  pref_registry->RegisterBooleanPref(autofill::prefs::kAutofillEnabled, false);
  pref_registry->RegisterBooleanPref(kDataReductionProxyEnabled, false);
  data_reduction_proxy::RegisterSimpleProfilePrefs(pref_registry);
  policy::URLBlacklistManager::RegisterProfilePrefs(pref_registry);

  pref_registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  pref_registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                                    std::string());

  metrics::MetricsService::RegisterPrefs(pref_registry);

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(make_scoped_refptr(new AwPrefStore()));
  pref_service_factory.set_managed_prefs(
      make_scoped_refptr(new policy::ConfigurationPolicyPrefStore(
          browser_policy_connector_->GetPolicyService(),
          browser_policy_connector_->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY)));
  pref_service_factory.set_read_error_callback(base::Bind(&HandleReadError));
  user_pref_service_ = pref_service_factory.Create(pref_registry);

  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

scoped_ptr<content::ZoomLevelDelegate>
AwBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath AwBrowserContext::GetPath() const {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() const {
  // Android WebView does not support off the record profile yet.
  return false;
}

net::URLRequestContextGetter* AwBrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* AwBrowserContext::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  NOTREACHED();
  return NULL;
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    resource_context_.reset(
        new AwResourceContext(url_request_context_getter_.get()));
  }
  return resource_context_.get();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  return &download_manager_delegate_;
}

content::BrowserPluginGuestManager* AwBrowserContext::GetGuestManager() {
  return NULL;
}

storage::SpecialStoragePolicy* AwBrowserContext::GetSpecialStoragePolicy() {
  // Intentionally returning NULL as 'Extensions' and 'Apps' not supported.
  return NULL;
}

content::PushMessagingService* AwBrowserContext::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in WebView.
  return NULL;
}

content::SSLHostStateDelegate* AwBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_.get()) {
    ssl_host_state_delegate_.reset(new AwSSLHostStateDelegate());
  }
  return ssl_host_state_delegate_.get();
}

content::PermissionManager* AwBrowserContext::GetPermissionManager() {
  if (!permission_manager_.get())
    permission_manager_.reset(new AwPermissionManager());
  return permission_manager_.get();
}

content::BackgroundSyncController*
AwBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

policy::URLBlacklistManager* AwBrowserContext::GetURLBlacklistManager() {
  // Should not be called until the end of PreMainMessageLoopRun, where
  // blacklist_manager_ is initialized.
  DCHECK(blacklist_manager_);
  return blacklist_manager_.get();
}

void AwBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

void AwBrowserContext::CreateDataReductionProxyStatisticsIfNecessary() {
  DCHECK(user_pref_service_.get());
  DCHECK(GetDataReductionProxySettings());
  data_reduction_proxy::DataReductionProxyService*
      data_reduction_proxy_service =
          GetDataReductionProxySettings()->data_reduction_proxy_service();
  DCHECK(data_reduction_proxy_service);
  if (data_reduction_proxy_service->compression_stats())
    return;
  // We don't care about commit_delay for now. It is just a dummy value.
  base::TimeDelta commit_delay = base::TimeDelta::FromMinutes(60);
  data_reduction_proxy_service->EnableCompressionStatisticsLogging(
      user_pref_service_.get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      commit_delay);
}

}  // namespace android_webview
