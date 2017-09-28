// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"

#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace media_router {

DialMediaSinkServiceImpl::DialMediaSinkServiceImpl(
    const OnSinksDiscoveredCallback& callback,
    net::URLRequestContextGetter* request_context)
    : MediaSinkServiceBase(callback),
      observer_(nullptr),
      request_context_(request_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_context_);
}

DialMediaSinkServiceImpl::~DialMediaSinkServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Stop();
}

void DialMediaSinkServiceImpl::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (dial_registry_)
    return;

  dial_registry_ =
      test_dial_registry_ ? test_dial_registry_ : DialRegistry::GetInstance();
  dial_registry_->SetNetLog(
      request_context_->GetURLRequestContext()->net_log());
  dial_registry_->RegisterObserver(this);
  dial_registry_->OnListenerAdded();
  MediaSinkServiceBase::StartTimer();
}

void DialMediaSinkServiceImpl::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!dial_registry_)
    return;

  dial_registry_->OnListenerRemoved();
  dial_registry_->UnregisterObserver(this);
  dial_registry_ = nullptr;
  MediaSinkServiceBase::StopTimer();
}

DeviceDescriptionService* DialMediaSinkServiceImpl::GetDescriptionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!description_service_.get()) {
    description_service_.reset(new DeviceDescriptionService(
        base::Bind(&DialMediaSinkServiceImpl::OnDeviceDescriptionAvailable,
                   base::Unretained(this)),
        base::Bind(&DialMediaSinkServiceImpl::OnDeviceDescriptionError,
                   base::Unretained(this))));
  }
  return description_service_.get();
}

void DialMediaSinkServiceImpl::SetObserver(
    DialMediaSinkServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observer_ = observer;
}

void DialMediaSinkServiceImpl::ClearObserver(
    DialMediaSinkServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observer_ = nullptr;
}

void DialMediaSinkServiceImpl::SetDialRegistryForTest(
    DialRegistry* dial_registry) {
  DCHECK(!test_dial_registry_);
  test_dial_registry_ = dial_registry;
}

void DialMediaSinkServiceImpl::SetDescriptionServiceForTest(
    std::unique_ptr<DeviceDescriptionService> description_service) {
  DCHECK(!description_service_);
  description_service_ = std::move(description_service);
}

void DialMediaSinkServiceImpl::OnDialDeviceEvent(
    const DialRegistry::DeviceList& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << "DialMediaSinkServiceImpl::OnDialDeviceEvent found "
           << devices.size() << " devices";

  current_sinks_.clear();
  current_devices_ = devices;

  GetDescriptionService()->GetDeviceDescriptions(devices,
                                                 request_context_.get());
}

void DialMediaSinkServiceImpl::OnDialError(DialRegistry::DialErrorCode type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << "OnDialError [DialErrorCode]: " << static_cast<int>(type);
}

void DialMediaSinkServiceImpl::OnDeviceDescriptionAvailable(
    const DialDeviceData& device_data,
    const ParsedDialDeviceDescription& description_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::ContainsValue(current_devices_, device_data)) {
    DVLOG(2) << "Device data not found in current device data list...";
    return;
  }

  // When use this "sink" within browser, please note it will have a different
  // ID when it is sent to the extension, because it derives a different sink ID
  // using the given sink ID.
  MediaSink sink(description_data.unique_id, description_data.friendly_name,
                 SinkIconType::GENERIC);
  DialSinkExtraData extra_data;
  extra_data.app_url = description_data.app_url;
  extra_data.model_name = description_data.model_name;
  std::string ip_address = device_data.device_description_url().host();
  if (!extra_data.ip_address.AssignFromIPLiteral(ip_address)) {
    DVLOG(1) << "Invalid ip_address: " << ip_address;
    return;
  }

  MediaSinkInternal dial_sink(sink, extra_data);
  current_sinks_.insert(dial_sink);
  if (observer_)
    observer_->OnDialSinkAdded(dial_sink);

  // Start fetch timer again if device description comes back after
  // |finish_timer_| fires.
  MediaSinkServiceBase::RestartTimer();
}

void DialMediaSinkServiceImpl::OnDeviceDescriptionError(
    const DialDeviceData& device,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(2) << "OnDescriptionFetchesError [message]: " << error_message;
}

void DialMediaSinkServiceImpl::RecordDeviceCounts() {
  metrics_.RecordDeviceCountsIfNeeded(current_sinks_.size(),
                                      current_devices_.size());
}

}  // namespace media_router
