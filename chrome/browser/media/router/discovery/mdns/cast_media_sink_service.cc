// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
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

namespace {

enum ErrorType {
  NONE,
  NOT_CAST_DEVICE,
  MISSING_ID,
  MISSING_FRIENDLY_NAME,
  MISSING_OR_INVALID_IP_ADDRESS,
  MISSING_OR_INVALID_PORT,
};

ErrorType CreateCastMediaSink(const media_router::DnsSdService& service,
                              media_router::MediaSinkInternal* cast_sink) {
  DCHECK(cast_sink);
  if (service.service_name.find(
          media_router::CastMediaSinkService::kCastServiceType) ==
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
  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::CAST);

  media_router::CastSinkExtraData extra_data;
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

namespace media_router {

// static
const char CastMediaSinkService::kCastServiceType[] = "_googlecast._tcp.local";

CastMediaSinkService::CastMediaSinkService(
    const OnSinksDiscoveredCallback& callback,
    content::BrowserContext* browser_context,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : MediaSinkService(callback),
      task_runner_(task_runner),
      browser_context_(browser_context) {
  // TODO(crbug.com/749305): Migrate the discovery code to use sequences.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(task_runner_);
}

CastMediaSinkService::CastMediaSinkService(
    const OnSinksDiscoveredCallback& callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<CastMediaSinkServiceImpl,
                    content::BrowserThread::DeleteOnIOThread>
        cast_media_sink_service_impl)
    : MediaSinkService(callback),
      task_runner_(task_runner),
      cast_media_sink_service_impl_(std::move(cast_media_sink_service_impl)) {}

CastMediaSinkService::~CastMediaSinkService() {}

void CastMediaSinkService::Start() {
  // TODO(crbug.com/749305): Migrate the discovery code to use sequences.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (dns_sd_registry_)
    return;

  if (!cast_media_sink_service_impl_) {
    cast_media_sink_service_impl_.reset(new CastMediaSinkServiceImpl(
        base::BindRepeating(&CastMediaSinkService::OnSinksDiscoveredOnIOThread,
                            this),
        cast_channel::CastSocketService::GetInstance(),
        DiscoveryNetworkMonitor::GetInstance(),
        Profile::FromBrowserContext(browser_context_)->GetRequestContext(),
        task_runner_));
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::Start,
                                cast_media_sink_service_impl_->AsWeakPtr()));

  dns_sd_registry_ = DnsSdRegistry::GetInstance();
  dns_sd_registry_->AddObserver(this);
  dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
}

void CastMediaSinkService::Stop() {
  // TODO(crbug.com/749305): Migrate the discovery code to use sequences.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!dns_sd_registry_)
    return;

  dns_sd_registry_->UnregisterDnsSdListener(kCastServiceType);
  dns_sd_registry_->RemoveObserver(this);
  dns_sd_registry_ = nullptr;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::Stop,
                                cast_media_sink_service_impl_->AsWeakPtr()));

  cast_media_sink_service_impl_.reset();
}

void CastMediaSinkService::ForceSinkDiscoveryCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!cast_media_sink_service_impl_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::ForceSinkDiscoveryCallback,
                     cast_media_sink_service_impl_->AsWeakPtr()));
}

void CastMediaSinkService::OnUserGesture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (dns_sd_registry_)
    dns_sd_registry_->ForceDiscovery();

  if (!cast_media_sink_service_impl_)
    return;

  DVLOG(2) << "OnUserGesture: open channel now for " << cast_sinks_.size()
           << " devices discovered in latest round of mDNS";
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::AttemptConnection,
                     cast_media_sink_service_impl_->AsWeakPtr(), cast_sinks_));
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

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::OpenChannels,
                     cast_media_sink_service_impl_->AsWeakPtr(), cast_sinks_,
                     CastMediaSinkServiceImpl::SinkSource::kMdns));
}

void CastMediaSinkService::OnDialSinkAdded(const MediaSinkInternal& sink) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  cast_media_sink_service_impl_->OnDialSinkAdded(sink);
}

void CastMediaSinkService::OnSinksDiscoveredOnIOThread(
    std::vector<MediaSinkInternal> sinks) {
  // TODO(crbug.com/749305): Migrate the discovery code to use sequences.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(sink_discovery_callback_, std::move(sinks)));
}

}  // namespace media_router
