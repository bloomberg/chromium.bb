// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "services/service_manager/public/cpp/connector.h"

namespace media_router {

DialMediaSinkServiceImpl::DialMediaSinkServiceImpl(
    std::unique_ptr<service_manager::Connector> connector,
    const OnSinksDiscoveredCallback& on_sinks_discovered_cb,
    const OnDialSinkAddedCallback& dial_sink_added_cb,
    const OnAvailableSinksUpdatedCallback& available_sinks_updated_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : MediaSinkServiceBase(on_sinks_discovered_cb),
      connector_(std::move(connector)),
      dial_sink_added_cb_(dial_sink_added_cb),
      available_sinks_updated_callback_(available_sinks_updated_callback),
      task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
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

  description_service_ = std::make_unique<DeviceDescriptionService>(
      connector_.get(),
      base::BindRepeating(
          &DialMediaSinkServiceImpl::OnDeviceDescriptionAvailable,
          base::Unretained(this)),
      base::BindRepeating(&DialMediaSinkServiceImpl::OnDeviceDescriptionError,
                          base::Unretained(this)));

  app_discovery_service_ = std::make_unique<DialAppDiscoveryService>(
      connector_.get(),
      base::BindRepeating(&DialMediaSinkServiceImpl::OnAppInfoParseCompleted,
                          base::Unretained(this)));

  dial_registry_ =
      test_dial_registry_ ? test_dial_registry_ : DialRegistry::GetInstance();
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
    for (const auto& sink_it : current_sinks_)
      dial_sink_added_cb_.Run(sink_it.second);
  }

  RescanAppInfo();
}

void DialMediaSinkServiceImpl::StartMonitoringAvailableSinksForApp(
    const std::string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!registered_apps_.insert(app_name).second)
    return;

  // Start checking if |app_name| is available on existing sinks.
  for (const auto& dial_sink_it : current_sinks_)
    FetchAppInfoForSink(dial_sink_it.second, app_name);
}

void DialMediaSinkServiceImpl::StopMonitoringAvailableSinksForApp(
    const std::string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registered_apps_.erase(app_name);
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

void DialMediaSinkServiceImpl::SetAppDiscoveryServiceForTest(
    std::unique_ptr<DialAppDiscoveryService> app_discovery_service) {
  app_discovery_service_ = std::move(app_discovery_service);
}

void DialMediaSinkServiceImpl::OnDiscoveryComplete() {
  MediaSinkServiceBase::OnDiscoveryComplete();
  for (const auto& app_name : registered_apps_)
    MaybeNotifySinkObservers(app_name);
}

void DialMediaSinkServiceImpl::OnDialDeviceEvent(
    const DialRegistry::DeviceList& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "DialMediaSinkServiceImpl::OnDialDeviceEvent found "
           << devices.size() << " devices";

  current_sinks_.clear();
  current_devices_ = devices;

  description_service_->GetDeviceDescriptions(devices);
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

  std::string processed_uuid =
      MediaSinkInternal::ProcessDeviceUUID(description_data.unique_id);
  std::string sink_id = base::StringPrintf("dial:<%s>", processed_uuid.c_str());
  MediaSink sink(sink_id, description_data.friendly_name, SinkIconType::GENERIC,
                 MediaRouteProviderId::EXTENSION);
  DialSinkExtraData extra_data;
  extra_data.app_url = description_data.app_url;
  extra_data.model_name = description_data.model_name;
  std::string ip_address = device_data.device_description_url().host();
  if (!extra_data.ip_address.AssignFromIPLiteral(ip_address)) {
    DVLOG(1) << "Invalid ip_address: " << ip_address;
    return;
  }

  MediaSinkInternal dial_sink(sink, extra_data);
  current_sinks_.insert_or_assign(sink_id, dial_sink);
  if (dial_sink_added_cb_)
    dial_sink_added_cb_.Run(dial_sink);

  // Start checking if all registered apps are available on |dial_sink|.
  for (const auto& app_name : registered_apps_)
    FetchAppInfoForSink(dial_sink, app_name);

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

void DialMediaSinkServiceImpl::OnAppInfoParseCompleted(
    const std::string& sink_id,
    const std::string& app_name,
    SinkAppStatus app_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::ContainsKey(registered_apps_, app_name)) {
    DVLOG(2) << "App name not registered: " << app_name;
    return;
  }

  SinkAppStatus old_status = GetAppStatus(sink_id, app_name);
  SetAppStatus(sink_id, app_name, app_status);

  if (old_status != app_status)
    MaybeNotifySinkObservers(app_name);
}

void DialMediaSinkServiceImpl::MaybeNotifySinkObservers(
    const std::string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<MediaSinkInternal> sinks = GetAvailableSinks(app_name);
  auto& last_known_sinks = last_known_available_sinks_[app_name];
  if (sinks == last_known_sinks)
    return;

  DVLOG(2) << "NotifySinkObservers " << app_name << " has [" << sinks.size()
           << "] sinks";
  available_sinks_updated_callback_.Run(
      app_name, std::vector<MediaSinkInternal>(sinks.begin(), sinks.end()));

  last_known_sinks.swap(sinks);
}

void DialMediaSinkServiceImpl::FetchAppInfoForSink(
    const MediaSinkInternal& dial_sink,
    const std::string& app_name) {
  std::string sink_id = dial_sink.sink().id();
  SinkAppStatus app_status = GetAppStatus(sink_id, app_name);
  if (app_status != SinkAppStatus::kUnknown)
    return;

  app_discovery_service_->FetchDialAppInfo(dial_sink, app_name);
}

void DialMediaSinkServiceImpl::RescanAppInfo() {
  for (const auto& dial_sink_it : current_sinks_) {
    for (const auto& app_name : registered_apps_) {
      FetchAppInfoForSink(dial_sink_it.second, app_name);
    }
  }
}

SinkAppStatus DialMediaSinkServiceImpl::GetAppStatus(
    const std::string& sink_id,
    const std::string& app_name) const {
  std::string key = sink_id + ':' + app_name;
  auto status_it = app_statuses_.find(key);
  return status_it == app_statuses_.end() ? SinkAppStatus::kUnknown
                                          : status_it->second;
}

void DialMediaSinkServiceImpl::SetAppStatus(const std::string& sink_id,
                                            const std::string& app_name,
                                            SinkAppStatus app_status) {
  std::string key = sink_id + ':' + app_name;
  app_statuses_[key] = app_status;
}

void DialMediaSinkServiceImpl::RecordDeviceCounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metrics_.RecordDeviceCountsIfNeeded(current_sinks_.size(),
                                      current_devices_.size());
}

base::flat_set<MediaSinkInternal> DialMediaSinkServiceImpl::GetAvailableSinks(
    const std::string& app_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<MediaSinkInternal> sinks;
  for (const auto& sink_it : current_sinks_) {
    std::string sink_id = sink_it.second.sink().id();
    if (GetAppStatus(sink_id, app_name) == SinkAppStatus::kAvailable)
      sinks.insert(sink_it.second);
  }
  return sinks;
}

}  // namespace media_router
