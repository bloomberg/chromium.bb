// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_proxy.h"

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/service_manager_connection.h"

using content::BrowserThread;

namespace media_router {

DialMediaSinkServiceProxy::DialMediaSinkServiceProxy(
    const MediaSinkService::OnSinksDiscoveredCallback& callback,
    content::BrowserContext* context)
    : MediaSinkService(callback), observer_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* profile = Profile::FromBrowserContext(context);
  request_context_ = profile->GetRequestContext();
  DCHECK(request_context_);
}

DialMediaSinkServiceProxy::~DialMediaSinkServiceProxy() = default;

void DialMediaSinkServiceProxy::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DialMediaSinkServiceProxy::StartOnIOThread, this,
                     CreateConnector()));
}

void DialMediaSinkServiceProxy::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DialMediaSinkServiceProxy::StopOnIOThread, this));
}

void DialMediaSinkServiceProxy::ForceSinkDiscoveryCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dial_media_sink_service_)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DialMediaSinkServiceImpl::ForceSinkDiscoveryCallback,
                     dial_media_sink_service_->AsWeakPtr()));
}

void DialMediaSinkServiceProxy::OnUserGesture() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dial_media_sink_service_)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DialMediaSinkServiceImpl::OnUserGesture,
                     dial_media_sink_service_->AsWeakPtr()));
}

void DialMediaSinkServiceProxy::SetObserver(
    DialMediaSinkServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_ = observer;
}

void DialMediaSinkServiceProxy::ClearObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_ = nullptr;
}

void DialMediaSinkServiceProxy::SetDialMediaSinkServiceForTest(
    std::unique_ptr<DialMediaSinkServiceImpl> dial_media_sink_service) {
  DCHECK(dial_media_sink_service);
  DCHECK(!dial_media_sink_service_);
  dial_media_sink_service_ = std::move(dial_media_sink_service);
}

std::unique_ptr<service_manager::Connector>
DialMediaSinkServiceProxy::CreateConnector() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Clone the connector so it can be used on the IO thread.
  return content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->Clone();
}

void DialMediaSinkServiceProxy::StartOnIOThread(
    std::unique_ptr<service_manager::Connector> connector) {
  if (!dial_media_sink_service_) {
    // Need to explicitly delete |dial_media_sink_service_| outside dtor to
    // avoid circular dependency.
    dial_media_sink_service_ = std::make_unique<DialMediaSinkServiceImpl>(
        std::move(connector),
        base::Bind(&DialMediaSinkServiceProxy::OnSinksDiscoveredOnIOThread,
                   this),
        request_context_.get());
    dial_media_sink_service_->SetObserver(observer_);
  }

  dial_media_sink_service_->Start();
}

void DialMediaSinkServiceProxy::StopOnIOThread() {
  if (!dial_media_sink_service_)
    return;

  dial_media_sink_service_->Stop();
  dial_media_sink_service_->ClearObserver();
  dial_media_sink_service_.reset();
}

void DialMediaSinkServiceProxy::OnSinksDiscoveredOnIOThread(
    std::vector<MediaSinkInternal> sinks) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(sink_discovery_callback_, std::move(sinks)));
}

}  // namespace media_router
