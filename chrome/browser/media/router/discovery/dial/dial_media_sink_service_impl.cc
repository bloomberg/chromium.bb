// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"

#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/service_manager/public/cpp/connector.h"

using content::BrowserThread;

namespace media_router {

DialMediaSinkServiceImpl::DialMediaSinkServiceImpl(
    std::unique_ptr<service_manager::Connector> connector,
    const OnSinksDiscoveredCallback& on_sinks_discovered_cb,
    const OnDialSinkAddedCallback& dial_sink_added_cb,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : MediaSinkServiceBase(on_sinks_discovered_cb),
      connector_(std::move(connector)),
      description_service_(std::make_unique<DeviceDescriptionService>(
          connector_.get(),
          base::Bind(&DialMediaSinkServiceImpl::OnDeviceDescriptionAvailable,
                     base::Unretained(this)),
          base::Bind(&DialMediaSinkServiceImpl::OnDeviceDescriptionError,
                     base::Unretained(this)))),
      dial_sink_added_cb_(dial_sink_added_cb),
      request_context_(request_context),
      task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(request_context_);
}

DialMediaSinkServiceImpl::~DialMediaSinkServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dial_registry_) {
    dial_registry_->OnListenerRemoved();
    dial_registry_->UnregisterObserver(this);
    dial_registry_ = nullptr;
  }
}

void DialMediaSinkServiceImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void DialMediaSinkServiceImpl::OnUserGesture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Re-sync sinks to CastMediaSinkService. It's possible that a DIAL-discovered
  // sink was added to CastMediaSinkService earlier, but was removed due to
  // flaky network. This gives CastMediaSinkService an opportunity to recover
  // even if mDNS is not working for some reason.
  DVLOG(2) << "OnUserGesture: re-syncing " << current_sinks_.size()
           << " sinks to CastMediaSinkService";

  if (dial_sink_added_cb_) {
    for (const auto& sink : current_sinks_)
      dial_sink_added_cb_.Run(sink);
  }
}

void DialMediaSinkServiceImpl::SetDialRegistryForTest(
    DialRegistry* dial_registry) {
  DCHECK(!test_dial_registry_);
  test_dial_registry_ = dial_registry;
}

void DialMediaSinkServiceImpl::SetDescriptionServiceForTest(
    std::unique_ptr<DeviceDescriptionService> description_service) {
  description_service_ = std::move(description_service);
}

void DialMediaSinkServiceImpl::OnDialDeviceEvent(
    const DialRegistry::DeviceList& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "DialMediaSinkServiceImpl::OnDialDeviceEvent found "
           << devices.size() << " devices";

  current_sinks_.clear();
  current_devices_ = devices;

  description_service_->GetDeviceDescriptions(devices, request_context_.get());
}

void DialMediaSinkServiceImpl::OnDialError(DialRegistry::DialErrorCode type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "OnDialError [DialErrorCode]: " << static_cast<int>(type);
}

void DialMediaSinkServiceImpl::OnDeviceDescriptionAvailable(
    const DialDeviceData& device_data,
    const ParsedDialDeviceDescription& description_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::ContainsValue(current_devices_, device_data)) {
    DVLOG(2) << "Device data not found in current device data list...";
    return;
  }

  // When use this "sink" within browser, please note it will have a different
  // ID when it is sent to the extension, because it derives a different sink ID
  // using the given sink ID.
  MediaSink sink(description_data.unique_id, description_data.friendly_name,
                 SinkIconType::GENERIC, MediaRouteProviderId::EXTENSION);
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
  if (dial_sink_added_cb_)
    dial_sink_added_cb_.Run(dial_sink);

  // Start fetch timer again if device description comes back after
  // |finish_timer_| fires.
  MediaSinkServiceBase::RestartTimer();
}

void DialMediaSinkServiceImpl::OnDeviceDescriptionError(
    const DialDeviceData& device,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "OnDeviceDescriptionError [message]: " << error_message;
}

void DialMediaSinkServiceImpl::RecordDeviceCounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_.RecordDeviceCountsIfNeeded(current_sinks_.size(),
                                      current_devices_.size());
}

}  // namespace media_router
