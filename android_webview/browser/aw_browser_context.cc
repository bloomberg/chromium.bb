// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_policy_connector.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_download_manager_delegate.h"
#include "android_webview/browser/aw_feature_list.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_metrics_service_client.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/aw_web_ui_controller_factory.h"
#include "android_webview/browser/cookie_manager.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_whitelist_manager.h"
#include "base/base_paths_posix.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/buildflags.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/public/cpp/features.h"
#include "services/preferences/tracked/segregated_pref_store.h"

using base::FilePath;
using content::BrowserThread;

namespace android_webview {

namespace {

const void* const kDownloadManagerDelegateKey = &kDownloadManagerDelegateKey;

AwBrowserContext* g_browser_context = NULL;

// Empty method to skip origin security check as DownloadManager will set its
// own method.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

}  // namespace

AwBrowserContext::AwBrowserContext()
    : context_storage_path_(GetContextStoragePath()),
      simple_factory_key_(GetPath(), IsOffTheRecord()) {
  DCHECK(!g_browser_context);
  g_browser_context = this;
  SimpleKeyMap::GetInstance()->Associate(this, &simple_factory_key_);

  BrowserContext::Initialize(this, context_storage_path_);

  // This constructor is entered during the creation of ContentBrowserClient,
  // before browser threads are created. Therefore any checks to enforce
  // threading (such as BrowserThread::CurrentlyOn()) will fail here.
}

AwBrowserContext::~AwBrowserContext() {
  DCHECK_EQ(this, g_browser_context);
  BrowserContext::NotifyWillBeDestroyed(this);
  SimpleKeyMap::GetInstance()->Dissociate(this);
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

base::FilePath AwBrowserContext::GetCacheDir() {
  FilePath cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_path)) {
    NOTREACHED() << "Failed to get app cache directory for Android WebView";
  }
  cache_path =
      cache_path.Append(FILE_PATH_LITERAL("org.chromium.android_webview"));
  return cache_path;
}

base::FilePath AwBrowserContext::GetPrefStorePath() {
  FilePath pref_store_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pref_store_path);
  // TODO(amalova): Assign a proper file path for non-default profiles
  // when we support multiple profiles
  pref_store_path =
      pref_store_path.Append(FILE_PATH_LITERAL("Default/Preferences"));

  return pref_store_path;
}

// static
base::FilePath AwBrowserContext::GetCookieStorePath() {
  FilePath cookie_store_path;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &cookie_store_path)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  cookie_store_path = cookie_store_path.Append(FILE_PATH_LITERAL("Cookies"));
  return cookie_store_path;
}

// static
base::FilePath AwBrowserContext::GetContextStoragePath() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }

  return user_data_dir;
}

// static
void AwBrowserContext::RegisterPrefs(PrefRegistrySimple* registry) {
  safe_browsing::RegisterProfilePrefs(registry);

  // Register the Autocomplete Data Retention Policy pref.
  // The default value '0' represents the latest Chrome major version on which
  // the retention policy ran. By setting it to a low default value, we're
  // making sure it runs now (as it only runs once per major version).
  registry->RegisterIntegerPref(
      autofill::prefs::kAutocompleteLastVersionRetentionPolicy, 0);

  // We only use the autocomplete feature of Autofill, which is controlled via
  // the manager_delegate. We don't use the rest of Autofill, which is why it is
  // hardcoded as disabled here.
  // TODO(crbug.com/873740): The following also disables autocomplete.
  // Investigate what the intended behavior is.
  registry->RegisterBooleanPref(autofill::prefs::kAutofillProfileEnabled,
                                false);
  registry->RegisterBooleanPref(autofill::prefs::kAutofillCreditCardEnabled,
                                false);

#if BUILDFLAG(ENABLE_MOJO_CDM)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
#endif
}

void AwBrowserContext::CreateUserPrefService() {
  browser_policy_connector_ = std::make_unique<AwBrowserPolicyConnector>();

  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  std::set<std::string> persistent_prefs;
  // Persisted to avoid having to provision MediaDrm every time the
  // application tries to play protected content after restart.
  persistent_prefs.insert(cdm::prefs::kMediaDrmStorage);

  pref_service_factory.set_user_prefs(base::MakeRefCounted<SegregatedPrefStore>(
      base::MakeRefCounted<InMemoryPrefStore>(),
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()), persistent_prefs,
      /*validation_delegate=*/nullptr));

  policy::URLBlacklistManager::RegisterProfilePrefs(pref_registry.get());
  pref_service_factory.set_managed_prefs(
      base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
          browser_policy_connector_.get(),
          browser_policy_connector_->GetPolicyService(),
          browser_policy_connector_->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY));

  user_pref_service_ = pref_service_factory.Create(pref_registry);

  // TODO(amalova): Do not call this method for non-default profile.
  MigrateLocalStatePrefs();

  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void AwBrowserContext::MigrateLocalStatePrefs() {
  PrefService* local_state = AwBrowserProcess::GetInstance()->local_state();
  if (!local_state->HasPrefPath(cdm::prefs::kMediaDrmStorage)) {
    return;
  }

  user_pref_service_->Set(cdm::prefs::kMediaDrmStorage,
                          *(local_state->Get(cdm::prefs::kMediaDrmStorage)));
  local_state->ClearPref(cdm::prefs::kMediaDrmStorage);
}

// static
std::vector<std::string> AwBrowserContext::GetAuthSchemes() {
  // In Chrome this is configurable via the AuthSchemes policy. For WebView
  // there is no interest to have it available so far.
  std::vector<std::string> supported_schemes = {"basic", "digest", "ntlm",
                                                "negotiate"};
  return supported_schemes;
}

void AwBrowserContext::PreMainMessageLoopRun() {
  CreateUserPrefService();

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // TODO(amalova): Create a new instance for non-default profiles.
    url_request_context_getter_ =
        AwBrowserProcess::GetInstance()->GetAwURLRequestContext();
  }

  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  visitedlink_master_.reset(
      new visitedlink::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();

  form_database_service_.reset(
      new AwFormDatabaseService(context_storage_path_));

  EnsureResourceContextInitialized(this);

  content::WebUIControllerFactory::RegisterFactory(
      AwWebUIControllerFactory::GetInstance());
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_.get()) {
    quota_manager_bridge_ = AwQuotaManagerBridge::Create(this);
  }
  return quota_manager_bridge_.get();
}

AwFormDatabaseService* AwBrowserContext::GetFormDatabaseService() {
  return form_database_service_.get();
}

autofill::AutocompleteHistoryManager*
AwBrowserContext::GetAutocompleteHistoryManager() {
  if (!autocomplete_history_manager_) {
    autocomplete_history_manager_ =
        std::make_unique<autofill::AutocompleteHistoryManager>();
    autocomplete_history_manager_->Init(
        form_database_service_->get_autofill_webdata_service(),
        user_pref_service_.get(), IsOffTheRecord());
  }

  return autocomplete_history_manager_.get();
}

base::FilePath AwBrowserContext::GetPath() {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() {
  // Android WebView does not support off the record profile yet.
  return false;
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    resource_context_.reset(new AwResourceContext);
  }
  return resource_context_.get();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  if (!GetUserData(kDownloadManagerDelegateKey)) {
    SetUserData(kDownloadManagerDelegateKey,
                std::make_unique<AwDownloadManagerDelegate>());
  }
  return static_cast<AwDownloadManagerDelegate*>(
      GetUserData(kDownloadManagerDelegateKey));
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

content::PermissionControllerDelegate*
AwBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_.reset(new AwPermissionManager());
  return permission_manager_.get();
}

content::ClientHintsControllerDelegate*
AwBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
AwBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
AwBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
AwBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* AwBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  // This function cannot actually create the request context because
  // there is a reentrant dependency on GetResourceContext() via
  // content::StoragePartitionImplMap::Create(). This is not fixable
  // until http://crbug.com/159193. Until then, assert that the context
  // has already been allocated and just handle setting the protocol_handlers.
  DCHECK(url_request_context_getter_);
  url_request_context_getter_->SetHandlersAndInterceptors(
      protocol_handlers, std::move(request_interceptors));
  return url_request_context_getter_.get();
}

net::URLRequestContextGetter* AwBrowserContext::CreateMediaRequestContext() {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  return AwBrowserProcess::GetInstance()->GetAwURLRequestContext();
}

download::InProgressDownloadManager*
AwBrowserContext::RetriveInProgressDownloadManager() {
  return new download::InProgressDownloadManager(
      nullptr, base::FilePath(),
      base::BindRepeating(&IgnoreOriginSecurityCheck),
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe), nullptr);
}

void AwBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

void AwBrowserContext::SetExtendedReportingAllowed(bool allowed) {
  user_pref_service_->SetBoolean(
      ::prefs::kSafeBrowsingExtendedReportingOptInAllowed, allowed);
}

// TODO(amalova): Make sure NetworkContext is configured correctly when
// off-the-record
network::mojom::NetworkContextParamsPtr
AwBrowserContext::GetNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = android_webview::GetUserAgent();

  // TODO(ntfschr): set this value to a proper value based on the user's
  // preferred locales (http://crbug.com/898555). For now, set this to
  // "en-US,en" instead of "en-us,en", since Android guarantees region codes
  // will be uppercase.
  context_params->accept_language =
      net::HttpUtil::GenerateAcceptLanguageHeader("en-US,en");

  // HTTP cache
  context_params->http_cache_enabled = true;
  context_params->http_cache_max_size = GetHttpCacheSize();
  context_params->http_cache_path = GetCacheDir();

  // WebView should persist and restore cookies between app sessions (including
  // session cookies).
  context_params->cookie_path = AwBrowserContext::GetCookieStorePath();
  context_params->restore_old_session_cookies = true;
  context_params->persist_session_cookies = true;
  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();
  context_params->cookie_manager_params->allow_file_scheme_cookies =
      CookieManager::GetInstance()->AllowFileSchemeCookies();

  context_params->initial_ssl_config = network::mojom::SSLConfig::New();
  // Allow SHA-1 to be used for locally-installed trust anchors, as WebView
  // should behave like the Android system would.
  context_params->initial_ssl_config->sha1_local_anchors_enabled = true;
  // Do not enforce the Legacy Symantec PKI policies outlined in
  // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html,
  // defer to the Android system.
  context_params->initial_ssl_config->symantec_enforcement_disabled = true;

  // WebView does not currently support Certificate Transparency
  // (http://crbug.com/921750).
  context_params->enforce_chrome_ct_policy = false;

  // WebView does not support ftp yet.
  context_params->enable_ftp_url_support = false;

  context_params->enable_brotli = base::FeatureList::IsEnabled(
      android_webview::features::kWebViewBrotliSupport);

  context_params->check_clear_text_permitted =
      AwContentBrowserClient::get_check_cleartext_permitted();

  // Update the cors_exempt_header_list to include internally-added headers, to
  // avoid triggering CORS checks.
  content::UpdateCorsExemptHeader(context_params.get());
  variations::UpdateCorsExemptHeaderForVariations(context_params.get());

  // Add proxy settings
  AwProxyConfigMonitor::GetInstance()->AddProxyToNetworkContextParams(
      context_params);

  return context_params;
}

}  // namespace android_webview
