// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_connection.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/audio_buildflags.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"

#if BUILDFLAG(USE_UNIX_SOCKETS)
#include "net/socket/unix_domain_client_socket_posix.h"
#else
#include "net/socket/tcp_client_socket.h"
#endif

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

constexpr base::TimeDelta kConnectTimeout = base::TimeDelta::FromSeconds(1);

}  // namespace

MixerConnection::MixerConnection() : weak_factory_(this) {}

MixerConnection::~MixerConnection() = default;

void MixerConnection::Connect() {
  DCHECK(!connecting_socket_);

#if BUILDFLAG(USE_UNIX_SOCKETS)
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string path =
      command_line->GetSwitchValueASCII(switches::kMixerServiceEndpoint);
  if (path.empty()) {
    path = kDefaultUnixDomainSocketPath;
  }
  connecting_socket_ = std::make_unique<net::UnixDomainClientSocket>(
      path, true /* use_abstract_namespace */);
#else   // BUILDFLAG(USE_UNIX_SOCKETS)
  int port = GetSwitchValueNonNegativeInt(switches::kMixerServiceEndpoint,
                                          kDefaultTcpPort);
  net::IPEndPoint endpoint(net::IPAddress::IPv4Localhost(), port);
  connecting_socket_ = std::make_unique<net::TCPClientSocket>(
      net::AddressList(endpoint), nullptr, nullptr, net::NetLogSource());
#endif  // BUILDFLAG(USE_UNIX_SOCKETS)

  int result = connecting_socket_->Connect(base::BindOnce(
      &MixerConnection::ConnectCallback, weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING) {
    ConnectCallback(result);
    return;
  }

  connection_timeout_.Start(FROM_HERE, kConnectTimeout, this,
                            &MixerConnection::ConnectTimeout);
}

void MixerConnection::ConnectCallback(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (!connecting_socket_) {
    return;
  }

  connection_timeout_.Stop();
  if (result == net::OK) {
    LOG_IF(INFO, !log_timeout_) << "Now connected to mixer service";
    log_connection_failure_ = true;
    log_timeout_ = true;
    OnConnected(std::move(connecting_socket_));
    return;
  }

  base::TimeDelta delay = kConnectTimeout;
  if (log_connection_failure_) {
    LOG(ERROR) << "Failed to connect to mixer service: " << result;
    log_connection_failure_ = false;
    delay = base::TimeDelta();  // No reconnect delay on the first failure.
  }
  connecting_socket_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MixerConnection::Connect, weak_factory_.GetWeakPtr()),
      delay);
}

void MixerConnection::ConnectTimeout() {
  if (!connecting_socket_) {
    return;
  }

  if (log_timeout_) {
    LOG(ERROR) << "Timed out connecting to mixer service";
    log_timeout_ = false;
  }
  connecting_socket_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MixerConnection::Connect, weak_factory_.GetWeakPtr()));
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
