// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_browser_context.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/mock_download_manager_delegate.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/geolocation/geolocation_permission_context.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/resource_context.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/shell/shell_browser_main.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/http/http_auth_handler_factory.gh"
#include "net/http/http_cache.h"
#include "net/base/origin_bound_cert_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#endif

namespace {

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      const FilePath& base_path_,
      MessageLoop* io_loop,
      MessageLoop* file_loop)
      : io_loop_(io_loop),
        file_loop_(file_loop) {
  }

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    if (!url_request_context_) {
      FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
      net::HttpCache::DefaultBackend* main_backend =
          new net::HttpCache::DefaultBackend(
              net::DISK_CACHE,
              cache_path,
              0,
              BrowserThread::GetMessageLoopProxyForThread(
                  BrowserThread::CACHE));

      net::NetLog* net_log = NULL;
      host_resolver_.reset(net::CreateSystemHostResolver(
          net::HostResolver::kDefaultParallelism,
          net::HostResolver::kDefaultRetryAttempts,
          net_log));

      cert_verifier_.reset(new net::CertVerifier());

      origin_bound_cert_service_.reset(new net::OriginBoundCertService(
          new net::DefaultOriginBoundCertStore(NULL)));

      dnsrr_resolver_.reset(new net::DnsRRResolver());

      proxy_config_service_.reset(
          net::ProxyService::CreateSystemProxyConfigService(
              io_loop_,
              file_loop_));

      // TODO(jam): use v8 if possible, look at chrome code.
      proxy_service_.reset(
          net::ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service_.get(),
          0,
          net_log));

      url_security_manager_.reset(net::URLSecurityManager::Create(NULL, NULL));

      std::vector<std::string> supported_schemes;
      base::SplitString("basic,digest,ntlm,negotiate", ',', &supported_schemes);
      http_auth_handler_factory_.reset(
          net::HttpAuthHandlerRegistryFactory::Create(
              supported_schemes,
              url_security_manager_.get(),
              host_resolver_.get(),
              std::string(),  // gssapi_library_name
              false,  // negotiate_disable_cname_lookup
              false)); // negotiate_enable_port

      net::HttpCache* main_cache = new net::HttpCache(
          host_resolver_.get(),
          cert_verifier_.get(),
          origin_bound_cert_service_.get(),
          dnsrr_resolver_.get(),
          NULL, //dns_cert_checker
          proxy_service_.get(),
          new net::SSLConfigServiceDefaults(),
          http_auth_handler_factory_.get(),
          NULL, // network_delegate
          net_log,
          main_backend);
      main_http_factory_.reset(main_cache);

      scoped_refptr<net::CookieStore> cookie_store =
          new net::CookieMonster(NULL, NULL);

      url_request_context_ = new net::URLRequestContext();
      job_factory_.reset(new net::URLRequestJobFactory);
      url_request_context_->set_job_factory(job_factory_.get());
      url_request_context_->set_http_transaction_factory(main_cache);
      url_request_context_->set_origin_bound_cert_service(
          origin_bound_cert_service_.get());
      url_request_context_->set_dnsrr_resolver(dnsrr_resolver_.get());
      url_request_context_->set_proxy_service(proxy_service_.get());
      url_request_context_->set_cookie_store(cookie_store);
    }

    return url_request_context_;
  }

  virtual net::CookieStore* DONTUSEME_GetCookieStore() OVERRIDE {
    if (BrowserThread::CurrentlyOn(BrowserThread::IO))
      return GetURLRequestContext()->cookie_store();
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }

  net::HostResolver* host_resolver() { return host_resolver_.get(); }

 private:
  FilePath base_path_;
  MessageLoop* io_loop_;
  MessageLoop* file_loop_;

  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  scoped_refptr<net::URLRequestContext> url_request_context_;

  scoped_ptr<net::HttpTransactionFactory> main_http_factory_;
  scoped_ptr<net::HostResolver> host_resolver_;
  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_ptr<net::OriginBoundCertService> origin_bound_cert_service_;
  scoped_ptr<net::DnsRRResolver> dnsrr_resolver_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::ProxyService> proxy_service_;
  scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
};

class ShellResourceContext : public content::ResourceContext {
 public:
  ShellResourceContext(ShellURLRequestContextGetter* getter)
      : getter_(getter) {
  }

 private:
  virtual void EnsureInitialized() const OVERRIDE {
    const_cast<ShellResourceContext*>(this)->InitializeInternal();
  }

  void InitializeInternal() {
    set_request_context(getter_->GetURLRequestContext());
    set_host_resolver(getter_->host_resolver());
  }

  scoped_refptr<ShellURLRequestContextGetter> getter_;
};

class ShellGeolocationPermissionContext : public GeolocationPermissionContext {
 public:
  ShellGeolocationPermissionContext() {
  }

  // GeolocationPermissionContext implementation).
  virtual void RequestGeolocationPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void CancelGeolocationPermissionRequest(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellGeolocationPermissionContext);
};

}  // namespace

namespace content {

ShellBrowserContext::ShellBrowserContext(
    ShellBrowserMainParts* shell_main_parts)
    : shell_main_parts_(shell_main_parts) {
}

ShellBrowserContext::~ShellBrowserContext() {
}

FilePath ShellBrowserContext::GetPath() {
  FilePath result;

#if defined(OS_WIN)
  CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &result));
  result.Append(std::wstring(L"content_shell"));
#else
  NOTIMPLEMENTED();
#endif

  if (!file_util::PathExists(result))
    file_util::CreateDirectory(result);

  return result;
}

bool ShellBrowserContext::IsOffTheRecord()  {
  return false;
}

SSLHostState* ShellBrowserContext::GetSSLHostState()  {
  if (!ssl_host_state_.get())
    ssl_host_state_.reset(new SSLHostState());
  return ssl_host_state_.get();
}

DownloadManager* ShellBrowserContext::GetDownloadManager()  {
  if (!download_manager_.get()) {
    download_status_updater_.reset(new DownloadStatusUpdater());

    download_manager_delegate_.reset(new MockDownloadManagerDelegate());
    download_manager_ = new DownloadManager(download_manager_delegate_.get(),
                                            download_status_updater_.get());
    download_manager_->Init(this);
  }
  return download_manager_.get();
}

bool ShellBrowserContext::HasCreatedDownloadManager() const  {
  return download_manager_.get() != NULL;
}

net::URLRequestContextGetter* ShellBrowserContext::GetRequestContext()  {
  if (!url_request_getter_) {
    url_request_getter_ = new ShellURLRequestContextGetter(
        GetPath(),
        shell_main_parts_->io_thread()->message_loop(),
        shell_main_parts_->file_thread()->message_loop());
  }
  return url_request_getter_;
}

net::URLRequestContextGetter*
    ShellBrowserContext::GetRequestContextForRenderProcess(
        int renderer_child_id)  {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    ShellBrowserContext::GetRequestContextForMedia()  {
  return GetRequestContext();
}

const ResourceContext& ShellBrowserContext::GetResourceContext()  {
  if (!resource_context_.get()) {
    resource_context_.reset(new ShellResourceContext(
        static_cast<ShellURLRequestContextGetter*>(GetRequestContext())));
  }
  return *resource_context_.get();
}

HostZoomMap* ShellBrowserContext::GetHostZoomMap()  {
  if (!host_zoom_map_)
    host_zoom_map_ = new HostZoomMap();
  return host_zoom_map_.get();
}

GeolocationPermissionContext*
    ShellBrowserContext::GetGeolocationPermissionContext()  {
  if (!geolocation_permission_context_) {
    geolocation_permission_context_ =
        new ShellGeolocationPermissionContext();
  }
  return geolocation_permission_context_;
}

bool ShellBrowserContext::DidLastSessionExitCleanly()  {
  return true;
}

quota::QuotaManager* ShellBrowserContext::GetQuotaManager()  {
  CreateQuotaManagerAndClients();
  return quota_manager_.get();
}

WebKitContext* ShellBrowserContext::GetWebKitContext()  {
  CreateQuotaManagerAndClients();
  return webkit_context_.get();
}

webkit_database::DatabaseTracker* ShellBrowserContext::GetDatabaseTracker()  {
  CreateQuotaManagerAndClients();
  return db_tracker_;
}

ChromeBlobStorageContext* ShellBrowserContext::GetBlobStorageContext()  {
  if (!blob_storage_context_) {
    blob_storage_context_ = new ChromeBlobStorageContext();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            blob_storage_context_.get(),
            &ChromeBlobStorageContext::InitializeOnIOThread));
  }
  return blob_storage_context_;
}

ChromeAppCacheService* ShellBrowserContext::GetAppCacheService()  {
  CreateQuotaManagerAndClients();
  return appcache_service_;
}

fileapi::FileSystemContext* ShellBrowserContext::GetFileSystemContext()  {
  CreateQuotaManagerAndClients();
  return file_system_context_.get();
}

void ShellBrowserContext::CreateQuotaManagerAndClients() {
  quota_manager_ = new quota::QuotaManager(
      IsOffTheRecord(),
      GetPath(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
      NULL);

  file_system_context_ = CreateFileSystemContext(
      GetPath(), IsOffTheRecord(), NULL, quota_manager_->proxy());
  db_tracker_ = new webkit_database::DatabaseTracker(
      GetPath(), IsOffTheRecord(), false, NULL, quota_manager_->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  webkit_context_ = new WebKitContext(
      IsOffTheRecord(), GetPath(), NULL, false, quota_manager_->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT));
  appcache_service_ = new ChromeAppCacheService(quota_manager_->proxy());
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy;
  BrowserThread::PostTask(
    BrowserThread::IO, FROM_HERE,
    NewRunnableMethod(
        appcache_service_.get(),
        &ChromeAppCacheService::InitializeOnIOThread,
        IsOffTheRecord()
            ? FilePath() : GetPath().Append(FILE_PATH_LITERAL("AppCache")),
        &GetResourceContext(),
        special_storage_policy));
}

}  // namespace content
