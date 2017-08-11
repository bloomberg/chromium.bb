// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service.h"
#include "chrome/common/media_router/media_sink.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/net_log/chrome_net_log.h"

namespace {

media_router::MediaSinkInternal CreateCastSinkFromDialSink(
    const media_router::MediaSinkInternal& dial_sink) {
  const std::string& unique_id = dial_sink.sink().id();
  const std::string& friendly_name = dial_sink.sink().name();
  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::CAST);

  media_router::CastSinkExtraData extra_data;
  extra_data.ip_address = dial_sink.dial_data().ip_address;
  extra_data.port = media_router::CastMediaSinkServiceImpl::kCastControlPort;
  extra_data.model_name = dial_sink.dial_data().model_name;
  extra_data.discovered_by_dial = true;
  extra_data.capabilities = cast_channel::CastDeviceCapability::NONE;

  return media_router::MediaSinkInternal(sink, extra_data);
}

}  // namespace

namespace media_router {

// static
const int CastMediaSinkServiceImpl::kCastControlPort = 8009;

CastMediaSinkServiceImpl::CastMediaSinkServiceImpl(
    const OnSinksDiscoveredCallback& callback,
    cast_channel::CastSocketService* cast_socket_service)
    : MediaSinkServiceBase(callback),
      cast_socket_service_(cast_socket_service) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(cast_socket_service_);
}

CastMediaSinkServiceImpl::~CastMediaSinkServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cast_channel::CastSocketService::GetInstance()->RemoveObserver(this);
}

// MediaSinkService implementation
void CastMediaSinkServiceImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaSinkServiceBase::StartTimer();
}

void CastMediaSinkServiceImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaSinkServiceBase::StopTimer();
}

void CastMediaSinkServiceImpl::OnFetchCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_sinks_.clear();

  // Copy cast sink from mDNS service to |current_sinks_|.
  for (const auto& sink_it : current_sinks_by_mdns_) {
    DVLOG(2) << "Discovered by mdns [name]: " << sink_it.second.sink().name()
             << " [ip_address]: "
             << sink_it.second.cast_data().ip_address.ToString();
    current_sinks_.insert(sink_it.second);
  }

  // Copy cast sink from DIAL discovery to |current_sinks_|.
  for (const auto& sink_it : current_sinks_by_dial_) {
    DVLOG(2) << "Discovered by dial [name]: " << sink_it.second.sink().name()
             << " [ip_address]: "
             << sink_it.second.cast_data().ip_address.ToString();
    if (!base::ContainsKey(current_sinks_by_mdns_, sink_it.first))
      current_sinks_.insert(sink_it.second);
  }

  MediaSinkServiceBase::OnFetchCompleted();
}

void CastMediaSinkServiceImpl::RecordDeviceCounts() {
  metrics_.RecordDeviceCountsIfNeeded(current_sinks_.size(),
                                      current_service_ip_endpoints_.size());
}

void CastMediaSinkServiceImpl::OpenChannels(
    std::vector<MediaSinkInternal> cast_sinks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_service_ip_endpoints_.clear();

  for (const auto& cast_sink : cast_sinks) {
    net::IPEndPoint ip_endpoint(cast_sink.cast_data().ip_address,
                                cast_sink.cast_data().port);
    current_service_ip_endpoints_.insert(ip_endpoint);
    OpenChannel(ip_endpoint, cast_sink);
  }

  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnError(const cast_channel::CastSocket& socket,
                                       cast_channel::ChannelError error_state) {
  DVLOG(1) << "OnError [ip_endpoint]: " << socket.ip_endpoint().ToString()
           << " [error_state]: "
           << cast_channel::ChannelErrorToString(error_state);
  net::IPEndPoint ip_endpoint = socket.ip_endpoint();
  current_sinks_by_dial_.erase(ip_endpoint);
  current_sinks_by_mdns_.erase(ip_endpoint);
  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnMessage(
    const cast_channel::CastSocket& socket,
    const cast_channel::CastMessage& message) {}

void CastMediaSinkServiceImpl::OpenChannel(const net::IPEndPoint& ip_endpoint,
                                           MediaSinkInternal cast_sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "OpenChannel " << ip_endpoint.ToString()
           << " name: " << cast_sink.sink().name();

  cast_socket_service_->OpenSocket(
      ip_endpoint, g_browser_process->net_log(),
      base::BindOnce(&CastMediaSinkServiceImpl::OnChannelOpened, AsWeakPtr(),
                     std::move(cast_sink)),
      this);
}

void CastMediaSinkServiceImpl::OnChannelOpened(
    MediaSinkInternal cast_sink,
    cast_channel::CastSocket* socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket);
  if (socket->error_state() != cast_channel::ChannelError::NONE) {
    DVLOG(2) << "Fail to open channel "
             << cast_sink.cast_data().ip_address.ToString()
             << " [name]: " << cast_sink.sink().name();
    return;
  }

  media_router::CastSinkExtraData extra_data = cast_sink.cast_data();
  extra_data.capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT;
  if (!socket->audio_only())
    extra_data.capabilities |= cast_channel::CastDeviceCapability::VIDEO_OUT;
  extra_data.cast_channel_id = socket->id();
  MediaSinkInternal updated_sink(cast_sink.sink(), extra_data);
  DVLOG(2) << "Ading sink to current_sinks_ [name]: "
           << updated_sink.sink().name();

  net::IPEndPoint ip_endpoint(cast_sink.cast_data().ip_address,
                              cast_sink.cast_data().port);
  // Add or update existing cast sink.
  if (updated_sink.cast_data().discovered_by_dial) {
    current_sinks_by_dial_[ip_endpoint] = updated_sink;
  } else {
    current_sinks_by_mdns_[ip_endpoint] = updated_sink;
  }
  MediaSinkServiceBase::RestartTimer();
}

void CastMediaSinkServiceImpl::OnDialSinkAdded(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net::IPEndPoint ip_endpoint(sink.dial_data().ip_address, kCastControlPort);

  if (base::ContainsKey(current_service_ip_endpoints_, ip_endpoint)) {
    DVLOG(2) << "Sink discovered by mDNS, skip adding [name]: "
             << sink.sink().name();
    return;
  }

  // TODO(crbug.com/753175): Dual discovery should not try to open cast channel
  // for non-Cast device.
  OpenChannel(ip_endpoint, CreateCastSinkFromDialSink(sink));
}

}  // namespace media_router
