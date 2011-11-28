// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_browser_context.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/download/download_id_factory.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/geolocation/geolocation_permission_context.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/speech/speech_input_preferences.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/shell/shell_browser_main.h"
#include "content/shell/shell_download_manager_delegate.h"
#include "content/shell/shell_resource_context.h"
#include "content/shell/shell_url_request_context_getter.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#endif

using content::BrowserThread;

namespace {

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

class ShellSpeechInputPreferences : public SpeechInputPreferences {
 public:
  ShellSpeechInputPreferences() {
  }

  // SpeechInputPreferences implementation.
  virtual bool filter_profanities() const OVERRIDE {
    return false;
  }

  virtual void set_filter_profanities(bool filter_profanities) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellSpeechInputPreferences);
};

}  // namespace

namespace content {

ShellBrowserContext::ShellBrowserContext(
    ShellBrowserMainParts* shell_main_parts)
    : download_id_factory_(new DownloadIdFactory(this)),
      shell_main_parts_(shell_main_parts) {
}

ShellBrowserContext::~ShellBrowserContext() {
  if (resource_context_.get()) {
    BrowserThread::DeleteSoon(
      BrowserThread::IO, FROM_HERE, resource_context_.release());
  }
}

FilePath ShellBrowserContext::GetPath() {
  if (!path_.empty())
    return path_;

#if defined(OS_WIN)
  CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &path_));
  path_ = path_.Append(std::wstring(L"content_shell"));
#else
  NOTIMPLEMENTED();
#endif

  if (!file_util::PathExists(path_))
    file_util::CreateDirectory(path_);

  return path_;
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

    download_manager_delegate_ = new ShellDownloadManagerDelegate();
    download_manager_ = new DownloadManagerImpl(download_manager_delegate_,
                                                download_id_factory_,
                                                download_status_updater_.get());
    download_manager_delegate_->SetDownloadManager(download_manager_.get());
    download_manager_->Init(this);
  }
  return download_manager_.get();
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
        static_cast<ShellURLRequestContextGetter*>(GetRequestContext()),
        GetBlobStorageContext(),
        download_id_factory_));
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

SpeechInputPreferences* ShellBrowserContext::GetSpeechInputPreferences() {
  if (!speech_input_preferences_.get())
    speech_input_preferences_ = new ShellSpeechInputPreferences();
  return speech_input_preferences_.get();
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
        base::Bind(
            &ChromeBlobStorageContext::InitializeOnIOThread,
            blob_storage_context_.get()));
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
  if (quota_manager_.get())
    return;
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
    base::Bind(
        &ChromeAppCacheService::InitializeOnIOThread,
        appcache_service_.get(),
        IsOffTheRecord()
            ? FilePath() : GetPath().Append(FILE_PATH_LITERAL("AppCache")),
        &GetResourceContext(),
        special_storage_policy));
}

}  // namespace content
