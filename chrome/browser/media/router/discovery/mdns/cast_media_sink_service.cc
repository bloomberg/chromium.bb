// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/media_router/media_sink.h"
#include "components/cast_channel/cast_socket_service.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/url_request/url_request_context_getter.h"

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
  MediaSink sink(unique_id, friendly_name, SinkIconType::CAST,
                 MediaRouteProviderId::EXTENSION);

  CastSinkExtraData extra_data;
  extra_data.ip_endpoint =
      net::IPEndPoint(ip_address, service.service_host_port.port());
  extra_data.model_name = service_data["md"];
  extra_data.capabilities = cast_channel::CastDeviceCapability::NONE;

  unsigned capacities;
  if (base::StringToUint(service_data["ca"], &capacities))
    extra_data.capabilities = capacities;

  cast_sink->set_sink(sink);
  cast_sink->set_cast_data(extra_data);

  return ErrorType::NONE;
}

}  // namespace

// static
const char CastMediaSinkService::kCastServiceType[] = "_googlecast._tcp.local";

CastMediaSinkService::CastMediaSinkService(
    content::BrowserContext* browser_context)
    : impl_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CastMediaSinkService::~CastMediaSinkService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (dns_sd_registry_) {
    dns_sd_registry_->UnregisterDnsSdListener(kCastServiceType);
    dns_sd_registry_->RemoveObserver(this);
    dns_sd_registry_ = nullptr;
  }
}

void CastMediaSinkService::Start(
    const OnSinksDiscoveredCallback& sinks_discovered_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!impl_);

  // |sinks_discovered_cb| should only be invoked on the current sequence.
  impl_ = CreateImpl(base::BindRepeating(&RunSinksDiscoveredCallbackOnSequence,
                                         base::SequencedTaskRunnerHandle::Get(),
                                         sinks_discovered_cb));
  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::Start,
                                base::Unretained(impl_.get())));

#if !defined(OS_WIN)
  StartMdnsDiscovery();
#endif
}

std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
CastMediaSinkService::CreateImpl(
    const OnSinksDiscoveredCallback& sinks_discovered_cb) {
  cast_channel::CastSocketService* cast_socket_service =
      cast_channel::CastSocketService::GetInstance();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      cast_socket_service->task_runner();
  return std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>(
      new CastMediaSinkServiceImpl(
          sinks_discovered_cb, cast_socket_service,
          DiscoveryNetworkMonitor::GetInstance(),
          Profile::FromBrowserContext(browser_context_)->GetRequestContext()),
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

void CastMediaSinkService::ForceSinkDiscoveryCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::ForceSinkDiscoveryCallback,
                     base::Unretained(impl_.get())));
}

void CastMediaSinkService::OnUserGesture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
  // TODO(crbug.com/749305): Migrate the discovery code to use sequences.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

scoped_refptr<base::SequencedTaskRunner>
CastMediaSinkService::GetImplTaskRunner() {
  return impl_->task_runner();
}

void CastMediaSinkService::OnDialSinkAdded(const MediaSinkInternal& sink) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::OnDialSinkAdded,
                                base::Unretained(impl_.get()), sink));
}

}  // namespace media_router
