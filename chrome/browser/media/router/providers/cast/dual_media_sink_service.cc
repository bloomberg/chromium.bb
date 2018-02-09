// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace media_router {

// static
DualMediaSinkService* DualMediaSinkService::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static DualMediaSinkService* instance = new DualMediaSinkService();
  return instance;
}

DualMediaSinkService::Subscription
DualMediaSinkService::AddSinksDiscoveredCallback(
    const OnSinksDiscoveredProviderCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sinks_discovered_callbacks_.Add(callback);
}

void DualMediaSinkService::OnUserGesture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dial_media_sink_service_->OnUserGesture();
  if (cast_media_sink_service_)
    cast_media_sink_service_->OnUserGesture();
}

void DualMediaSinkService::StartMdnsDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cast_media_sink_service_)
    cast_media_sink_service_->StartMdnsDiscovery();
}

void DualMediaSinkService::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dial_media_sink_service_->RegisterMediaSinksObserver(observer);
}

void DualMediaSinkService::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dial_media_sink_service_->UnregisterMediaSinksObserver(observer);
}

DualMediaSinkService::DualMediaSinkService() {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      g_browser_process->system_request_context();

  if (media_router::CastDiscoveryEnabled()) {
    cast_media_sink_service_ =
        std::make_unique<CastMediaSinkService>(request_context);
    cast_media_sink_service_->Start(
        base::BindRepeating(&DualMediaSinkService::OnSinksDiscovered,
                            base::Unretained(this), "cast"));
  }

  OnDialSinkAddedCallback dial_sink_added_cb;
  if (cast_media_sink_service_)
    dial_sink_added_cb = cast_media_sink_service_->GetDialSinkAddedCallback();

  dial_media_sink_service_ =
      std::make_unique<DialMediaSinkService>(request_context);
  dial_media_sink_service_->Start(
      base::BindRepeating(&DualMediaSinkService::OnSinksDiscovered,
                          base::Unretained(this), "dial"),
      dial_sink_added_cb);
}

DualMediaSinkService::DualMediaSinkService(
    std::unique_ptr<CastMediaSinkService> cast_media_sink_service,
    std::unique_ptr<DialMediaSinkService> dial_media_sink_service)
    : cast_media_sink_service_(std::move(cast_media_sink_service)),
      dial_media_sink_service_(std::move(dial_media_sink_service)) {}

DualMediaSinkService::~DualMediaSinkService() = default;

void DualMediaSinkService::OnSinksDiscovered(
    const std::string& provider_name,
    std::vector<MediaSinkInternal> sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& sinks_for_provider = current_sinks_[provider_name];
  sinks_for_provider = std::move(sinks);
  sinks_discovered_callbacks_.Notify(provider_name, sinks_for_provider);
}

}  // namespace media_router
