// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/logging_network_change_observer.h"

#include <string>

#include "base/logging.h"
#include "net/log/net_log.h"

namespace net {

LoggingNetworkChangeObserver::LoggingNetworkChangeObserver(net::NetLog* net_log)
    : net_log_(net_log) {
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

LoggingNetworkChangeObserver::~LoggingNetworkChangeObserver() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void LoggingNetworkChangeObserver::OnIPAddressChanged() {
  VLOG(1) << "Observed a change to the network IP addresses";

  net_log_->AddGlobalEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSES_CHANGED);
}

void LoggingNetworkChangeObserver::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  std::string type_as_string =
      net::NetworkChangeNotifier::ConnectionTypeToString(type);

  VLOG(1) << "Observed a change to network connectivity state "
          << type_as_string;

  net_log_->AddGlobalEntry(
      net::NetLog::TYPE_NETWORK_CONNECTIVITY_CHANGED,
      net::NetLog::StringCallback("new_connection_type", &type_as_string));
}

void LoggingNetworkChangeObserver::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  std::string type_as_string =
      net::NetworkChangeNotifier::ConnectionTypeToString(type);

  VLOG(1) << "Observed a network change to state " << type_as_string;

  net_log_->AddGlobalEntry(
      net::NetLog::TYPE_NETWORK_CHANGED,
      net::NetLog::StringCallback("new_connection_type", &type_as_string));
}

}  // namespace net
