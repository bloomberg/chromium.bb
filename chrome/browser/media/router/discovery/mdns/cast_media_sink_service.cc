// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/net_log/chrome_net_log.h"
#include "content/public/common/content_client.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"

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
                              int channel_id,
                              bool audio_only,
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
  extra_data.ip_address = ip_address;
  extra_data.model_name = service_data["md"];
  extra_data.capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT;
  if (!audio_only)
    extra_data.capabilities |= cast_channel::CastDeviceCapability::VIDEO_OUT;
  extra_data.cast_channel_id = channel_id;

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
    content::BrowserContext* browser_context)
    : MediaSinkServiceBase(callback),
      cast_socket_service_(cast_channel::CastSocketService::GetInstance()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cast_socket_service_);
}

CastMediaSinkService::CastMediaSinkService(
    const OnSinksDiscoveredCallback& callback,
    cast_channel::CastSocketService* cast_socket_service)
    : MediaSinkServiceBase(callback),
      cast_socket_service_(cast_socket_service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cast_socket_service_);
}

CastMediaSinkService::~CastMediaSinkService() {}

void CastMediaSinkService::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (dns_sd_registry_)
    return;

  dns_sd_registry_ = DnsSdRegistry::GetInstance();
  dns_sd_registry_->AddObserver(this);
  dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
  MediaSinkServiceBase::StartTimer();
}

void CastMediaSinkService::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!dns_sd_registry_)
    return;

  dns_sd_registry_->UnregisterDnsSdListener(kCastServiceType);
  dns_sd_registry_->RemoveObserver(this);
  dns_sd_registry_ = nullptr;
  MediaSinkServiceBase::StopTimer();
}

void CastMediaSinkService::SetDnsSdRegistryForTest(DnsSdRegistry* registry) {
  DCHECK(!dns_sd_registry_);
  dns_sd_registry_ = registry;
  dns_sd_registry_->AddObserver(this);
  dns_sd_registry_->RegisterDnsSdListener(kCastServiceType);
  MediaSinkServiceBase::StartTimer();
}

void CastMediaSinkService::OnDnsSdEvent(
    const std::string& service_type,
    const DnsSdRegistry::DnsSdServiceList& services) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << "CastMediaSinkService::OnDnsSdEvent found " << services.size()
           << " services";

  current_sinks_.clear();
  current_services_ = services;

  for (const auto& service : services) {
    net::IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(service.ip_address)) {
      DVLOG(2) << "Invalid ip_addresss: " << service.ip_address;
      continue;
    }
    net::HostPortPair host_port_pair =
        net::HostPortPair::FromString(service.service_host_port);

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&CastMediaSinkService::OpenChannelOnIOThread, this, service,
                   net::IPEndPoint(ip_address, host_port_pair.port())));
  }

  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkService::OpenChannelOnIOThread(
    const DnsSdService& service,
    const net::IPEndPoint& ip_endpoint) {
  if (!observer_)
    observer_.reset(new CastSocketObserver());

  cast_socket_service_->OpenSocket(
      ip_endpoint, g_browser_process->net_log(),
      base::Bind(&CastMediaSinkService::OnChannelOpenedOnIOThread, this,
                 service),
      observer_.get());
}

void CastMediaSinkService::OnChannelOpenedOnIOThread(
    const DnsSdService& service,
    int channel_id,
    cast_channel::ChannelError channel_error) {
  if (channel_error != cast_channel::ChannelError::NONE) {
    DVLOG(2) << "Fail to open channel " << service.ip_address << ": "
             << service.service_host_port
             << " [ChannelError]: " << (int)channel_error;
    return;
  }

  auto* socket = cast_socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << "Fail to find socket with [channel_id]: " << channel_id;
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&CastMediaSinkService::OnChannelOpenedOnUIThread, this,
                 service, channel_id, socket->audio_only()));
}

void CastMediaSinkService::OnChannelOpenedOnUIThread(
    const DnsSdService& service,
    int channel_id,
    bool audio_only) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MediaSinkInternal sink;
  ErrorType error = CreateCastMediaSink(service, channel_id, audio_only, &sink);
  if (error != ErrorType::NONE) {
    DVLOG(2) << "Fail to create Cast device [error]: " << error;
    return;
  }

  if (!base::ContainsValue(current_services_, service)) {
    DVLOG(2) << "Service data not found in current service data list...";
    return;
  }

  DVLOG(2) << "Ading sink to current_sinks_ [id]: " << sink.sink().id();
  current_sinks_.insert(sink);
  MediaSinkServiceBase::RestartTimer();
}

CastMediaSinkService::CastSocketObserver::CastSocketObserver() {}
CastMediaSinkService::CastSocketObserver::~CastSocketObserver() {
  cast_channel::CastSocketService::GetInstance()->RemoveObserver(this);
}

void CastMediaSinkService::CastSocketObserver::OnError(
    const cast_channel::CastSocket& socket,
    cast_channel::ChannelError error_state) {
  DVLOG(1) << "OnError [ip_endpoint]: " << socket.ip_endpoint().ToString()
           << " [error_state]: "
           << cast_channel::ChannelErrorToString(error_state);
}

void CastMediaSinkService::CastSocketObserver::OnMessage(
    const cast_channel::CastSocket& socket,
    const cast_channel::CastMessage& message) {}

}  // namespace media_router
