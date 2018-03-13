// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/media_router/media_sink.h"
#include "components/cast_channel/cast_socket_service.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"

namespace media_router {

namespace {

enum ErrorType {
  NONE,
  NOT_CAST_DEVICE,
  MISSING_ID,
  MISSING_FRIENDLY_NAME,
  MISSING_OR_INVALID_IP_ADDRESS,
  MISSING_OR_INVALID_PORT,
};

ErrorType CreateCastMediaSink(const DnsSdService& service,
                              MediaSinkInternal* cast_sink) {
  DCHECK(cast_sink);
  if (service.service_name.find(CastMediaSinkService::kCastServiceType) ==
      std::string::npos)
    return ErrorType::NOT_CAST_DEVICE;

  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(service.ip_address))
    return ErrorType::MISSING_OR_INVALID_IP_ADDRESS;

  std::map<std::string, std::string> service_data;
  for (const auto& item : service.service_data) {
    // |item| format should be "id=xxxxxx", etc.
    size_t split_idx = item.find('=');
    if (split_idx == std::string::npos)
      continue;

    std::string key = item.substr(0, split_idx);
    std::string val =
        split_idx < item.length() ? item.substr(split_idx + 1) : "";
    service_data[key] = val;
  }

  // When use this "sink" within browser, please note it will have a different
  // ID when it is sent to the extension, because it derives a different sink ID
  // using the given sink ID.
  std::string unique_id = service_data["id"];
  if (unique_id.empty())
    return ErrorType::MISSING_ID;
  std::string friendly_name = service_data["fn"];
  if (friendly_name.empty())
    return ErrorType::MISSING_FRIENDLY_NAME;

  CastSinkExtraData extra_data;
  extra_data.ip_endpoint =
      net::IPEndPoint(ip_address, service.service_host_port.port());
  extra_data.model_name = service_data["md"];
  extra_data.capabilities = cast_channel::CastDeviceCapability::NONE;

  unsigned capacities;
  if (base::StringToUint(service_data["ca"], &capacities))
    extra_data.capabilities = capacities;

  std::string processed_uuid = MediaSinkInternal::ProcessDeviceUUID(unique_id);
  std::string sink_id = base::StringPrintf("cast:<%s>", processed_uuid.c_str());
  MediaSink sink(
      sink_id, friendly_name,
      CastMediaSinkServiceImpl::GetCastSinkIconType(extra_data.capabilities),
      MediaRouteProviderId::CAST);

  cast_sink->set_sink(sink);
  cast_sink->set_cast_data(extra_data);

  return ErrorType::NONE;
}

}  // namespace

// static
const char CastMediaSinkService::kCastServiceType[] = "_googlecast._tcp.local";

CastMediaSinkService::CastMediaSinkService()
    : impl_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      weak_ptr_factory_(this) {}

CastMediaSinkService::~CastMediaSinkService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dns_sd_registry_) {
    dns_sd_registry_->UnregisterDnsSdListener(kCastServiceType);
    dns_sd_registry_->RemoveObserver(this);
    dns_sd_registry_ = nullptr;
  }
}

void CastMediaSinkService::Start(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    CastMediaSinkServiceImpl::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!impl_);

  // |sinks_discovered_cb| should only be invoked on the current sequence.
  // We wrap |sinks_discovered_cb| in a member function bound with WeakPtr to
  // ensure it will only be invoked while |this| is still valid.
  // TODO(imcheng): Simplify this by only using observers instead of callback.
  // This would require us to move the timer logic from MediaSinkServiceBase up
  // to DualMediaSinkService, but will allow us to remove MediaSinkServiceBase.
  impl_ =
      CreateImpl(base::BindRepeating(
                     &RunSinksDiscoveredCallbackOnSequence,
                     base::SequencedTaskRunnerHandle::Get(),
                     base::BindRepeating(
                         &CastMediaSinkService::RunSinksDiscoveredCallback,
                         weak_ptr_factory_.GetWeakPtr(), sinks_discovered_cb)),
                 observer);
  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::Start,
                                base::Unretained(impl_.get())));

#if !defined(OS_WIN)
  StartMdnsDiscovery();
#endif
}

std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
CastMediaSinkService::CreateImpl(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    CastMediaSinkServiceImpl::Observer* observer) {
  cast_channel::CastSocketService* cast_socket_service =
      cast_channel::CastSocketService::GetInstance();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      cast_socket_service->task_runner();
  return std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>(
      new CastMediaSinkServiceImpl(sinks_discovered_cb, observer,
                                   cast_socket_service,
                                   DiscoveryNetworkMonitor::GetInstance()),
      base::OnTaskRunnerDeleter(task_runner));
}

void CastMediaSinkService::StartMdnsDiscovery() {
  // |dns_sd_registry_| is already set to a mock version in unit tests only.
  // |impl_| must be initialized first because AddObserver might end up calling
  // |OnDnsSdEvent| right away.
  DCHECK(impl_);
  if (!dns_sd_registry_) {
    dns_sd_registry_ = DnsSdRegistry::GetInstance();
    dns_sd_registry_->AddObserver(this);
    dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
  }
}

void CastMediaSinkService::OnUserGesture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dns_sd_registry_)
    dns_sd_registry_->ForceDiscovery();

  DVLOG(2) << "OnUserGesture: open channel now for " << cast_sinks_.size()
           << " devices discovered in latest round of mDNS";
  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::AttemptConnection,
                                base::Unretained(impl_.get()), cast_sinks_));
}

void CastMediaSinkService::SetDnsSdRegistryForTest(DnsSdRegistry* registry) {
  DCHECK(!dns_sd_registry_);
  dns_sd_registry_ = registry;
  dns_sd_registry_->AddObserver(this);
  dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
}

void CastMediaSinkService::OnDnsSdEvent(
    const std::string& service_type,
    const DnsSdRegistry::DnsSdServiceList& services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "CastMediaSinkService::OnDnsSdEvent found " << services.size()
           << " services";

  cast_sinks_.clear();

  for (const auto& service : services) {
    // Create Cast sink from mDNS service description.
    MediaSinkInternal cast_sink;
    ErrorType error = CreateCastMediaSink(service, &cast_sink);
    if (error != ErrorType::NONE) {
      DVLOG(2) << "Fail to create Cast device [error]: " << error;
      continue;
    }

    cast_sinks_.push_back(cast_sink);
  }

  impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannelsWithRandomizedDelay,
                     base::Unretained(impl_.get()), cast_sinks_,
                     CastMediaSinkServiceImpl::SinkSource::kMdns));
}

OnDialSinkAddedCallback CastMediaSinkService::GetDialSinkAddedCallback() {
  return impl_->GetDialSinkAddedCallback();
}

void CastMediaSinkService::RunSinksDiscoveredCallback(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    std::vector<MediaSinkInternal> sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sinks_discovered_cb.Run(std::move(sinks));
}

}  // namespace media_router
