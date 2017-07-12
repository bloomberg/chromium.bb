// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"

#include <unordered_set>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/router/discovery/discovery_network_list.h"
#include "net/base/network_interfaces.h"

namespace {

using content::BrowserThread;

std::string ComputeNetworkId(
    const std::vector<DiscoveryNetworkInfo>& network_info_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (network_info_list.size() == 0) {
    return DiscoveryNetworkMonitor::kNetworkIdDisconnected;
  }
  if (std::find_if(network_info_list.begin(), network_info_list.end(),
                   [](const DiscoveryNetworkInfo& network_info) {
                     return network_info.network_id.size() > 0;
                   }) == network_info_list.end()) {
    return DiscoveryNetworkMonitor::kNetworkIdUnknown;
  }

  std::string combined_ids;
  for (const auto& network_info : network_info_list) {
    combined_ids = combined_ids + "!" + network_info.network_id;
  }

  std::string hash = base::SHA1HashString(combined_ids);
  return base::ToLowerASCII(base::HexEncode(hash.data(), hash.length()));
}

std::vector<DiscoveryNetworkInfo> GetNetworkInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return GetDiscoveryNetworkInfoList();
}

base::LazyInstance<DiscoveryNetworkMonitor>::Leaky g_discovery_monitor;

}  // namespace

// static
constexpr char const DiscoveryNetworkMonitor::kNetworkIdDisconnected[];
// static
constexpr char const DiscoveryNetworkMonitor::kNetworkIdUnknown[];

// static
DiscoveryNetworkMonitor* DiscoveryNetworkMonitor::GetInstance() {
  return g_discovery_monitor.Pointer();
}

void DiscoveryNetworkMonitor::RebindNetworkChangeObserverForTest() {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

void DiscoveryNetworkMonitor::SetNetworkInfoFunctionForTest(
    NetworkInfoFunction strategy) {
  network_info_function_ = strategy;
}

void DiscoveryNetworkMonitor::AddObserver(Observer* const observer) {
  observers_->AddObserver(observer);
}

void DiscoveryNetworkMonitor::RemoveObserver(Observer* const observer) {
  observers_->RemoveObserver(observer);
}

void DiscoveryNetworkMonitor::Refresh(NetworkRefreshCompleteCallback callback) {
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DiscoveryNetworkMonitor::UpdateNetworkInfo,
                     base::Unretained(this)),
      std::move(callback));
}

const std::string& DiscoveryNetworkMonitor::GetNetworkId() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return network_id_;
}

DiscoveryNetworkMonitor::DiscoveryNetworkMonitor()
    : network_id_(kNetworkIdDisconnected),
      observers_(new base::ObserverListThreadSafe<Observer>(
          base::ObserverListThreadSafe<
              Observer>::NotificationType::NOTIFY_EXISTING_ONLY)),
      network_info_function_(&GetNetworkInfo) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DiscoveryNetworkMonitor::~DiscoveryNetworkMonitor() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void DiscoveryNetworkMonitor::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DiscoveryNetworkMonitor::UpdateNetworkInfo,
                     base::Unretained(this)));
}

void DiscoveryNetworkMonitor::UpdateNetworkInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto network_info_list = network_info_function_();
  auto network_id = ComputeNetworkId(network_info_list);

  network_id_.swap(network_id);

  if (network_id_ != network_id) {
    observers_->Notify(FROM_HERE, &Observer::OnNetworksChanged,
                       base::ConstRef(*this));
  }
}
