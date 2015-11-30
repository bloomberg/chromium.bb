// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_browser_context.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/path_service.h"
#include "blimp/engine/browser/blimp_permission_manager.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"

namespace blimp {
namespace engine {

// Contains URLRequestContextGetter required for resource loading.
class BlimpResourceContext : public content::ResourceContext {
 public:
  BlimpResourceContext() {}
  ~BlimpResourceContext() override {}

  void set_url_request_context_getter(
      const scoped_refptr<BlimpURLRequestContextGetter>& getter) {
    getter_ = getter;
  }

  const scoped_refptr<BlimpURLRequestContextGetter>&
  url_request_context_getter() {
    return getter_;
  }

  // content::ResourceContext implementation.
  net::HostResolver* GetHostResolver() override;
  net::URLRequestContext* GetRequestContext() override;

 private:
  scoped_refptr<BlimpURLRequestContextGetter> getter_;

  DISALLOW_COPY_AND_ASSIGN(BlimpResourceContext);
};

net::HostResolver* BlimpResourceContext::GetHostResolver() {
  return getter_->host_resolver();
}

net::URLRequestContext* BlimpResourceContext::GetRequestContext() {
  return getter_->GetURLRequestContext();
}

BlimpBrowserContext::BlimpBrowserContext(bool off_the_record,
                                         net::NetLog* net_log)
    : resource_context_(new BlimpResourceContext),
      ignore_certificate_errors_(false),
      off_the_record_(off_the_record),
      net_log_(net_log) {
  InitWhileIOAllowed();
}

BlimpBrowserContext::~BlimpBrowserContext() {
  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       resource_context_.release());
  }
}

void BlimpBrowserContext::InitWhileIOAllowed() {
  // Ensures ~/.config/blimp_engine directory exists.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  path_ = config_dir.Append("blimp_engine");
  if (!base::PathExists(path_))
    base::CreateDirectory(path_);
}

scoped_ptr<content::ZoomLevelDelegate>
BlimpBrowserContext::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}

base::FilePath BlimpBrowserContext::GetPath() const {
  return path_;
}

bool BlimpBrowserContext::IsOffTheRecord() const {
  return off_the_record_;
}

content::DownloadManagerDelegate*
BlimpBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* BlimpBrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

const scoped_refptr<BlimpURLRequestContextGetter>&
BlimpBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!resource_context_->url_request_context_getter());
  // net_log_ is owned by BrowserMainParts.
  resource_context_->set_url_request_context_getter(
      new BlimpURLRequestContextGetter(
          ignore_certificate_errors_, GetPath(),
          content::BrowserThread::UnsafeGetMessageLoopForThread(
              content::BrowserThread::IO)
              ->task_runner(),
          content::BrowserThread::UnsafeGetMessageLoopForThread(
              content::BrowserThread::FILE)
              ->task_runner(),
          protocol_handlers, std::move(request_interceptors), net_log_));
  return resource_context_->url_request_context_getter();
}

net::URLRequestContextGetter*
BlimpBrowserContext::GetRequestContextForRenderProcess(int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* BlimpBrowserContext::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
BlimpBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
BlimpBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return GetRequestContext();
}

content::ResourceContext* BlimpBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::BrowserPluginGuestManager* BlimpBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* BlimpBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService* BlimpBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate* BlimpBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionManager* BlimpBrowserContext::GetPermissionManager() {
  if (!permission_manager_)
    permission_manager_.reset(new BlimpPermissionManager());
  return permission_manager_.get();
}

content::BackgroundSyncController*
BlimpBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

}  // namespace engine
}  // namespace blimp
