// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/arc_net_host_impl.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/arc/arc_bridge_service.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace {

const int kGetNetworksListLimit = 100;

}  // namespace

namespace arc {

ArcNetHostImpl::ArcNetHostImpl(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->AddObserver(this);
}

ArcNetHostImpl::~ArcNetHostImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->RemoveObserver(this);
}

void ArcNetHostImpl::OnNetInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());

  NetHostPtr host;
  binding_.Bind(GetProxy(&host));
  arc_bridge_service()->net_instance()->Init(std::move(host));
}

void ArcNetHostImpl::GetNetworks(bool configured_only,
                                 bool visible_only,
                                 const GetNetworksCallback& callback) {
  NetworkDataPtr data = NetworkData::New();
  data->status = NetworkResult::SUCCESS;

  // Retrieve list of nearby wifi networks
  chromeos::NetworkTypePattern network_pattern =
      chromeos::onc::NetworkTypePatternFromOncType(onc::network_type::kWiFi);
  scoped_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          network_pattern, configured_only, visible_only,
          kGetNetworksListLimit);

  // Extract info for each network and add it to the list.
  for (base::Value* value : *network_properties_list) {
    WifiConfigurationPtr wc = WifiConfiguration::New();

    base::DictionaryValue* network_dict = nullptr;
    value->GetAsDictionary(&network_dict);
    DCHECK(network_dict);

    // kName is a post-processed version of kHexSSID.
    std::string tmp;
    network_dict->GetString(onc::network_config::kName, &tmp);
    DCHECK(!tmp.empty());
    wc->ssid = tmp;

    base::DictionaryValue* wifi_dict = nullptr;
    network_dict->GetDictionary(onc::network_config::kWiFi, &wifi_dict);
    DCHECK(wifi_dict);

    if (!wifi_dict->GetInteger(onc::wifi::kFrequency, &wc->frequency))
      wc->frequency = 0;
    if (!wifi_dict->GetInteger(onc::wifi::kSignalStrength,
                               &wc->signal_strength))
      wc->signal_strength = 0;

    if (!wifi_dict->GetString(onc::wifi::kSecurity, &tmp))
      NOTREACHED();
    DCHECK(!tmp.empty());
    wc->security = tmp;

    if (!wifi_dict->GetString(onc::wifi::kBSSID, &tmp))
      NOTREACHED();
    DCHECK(!tmp.empty());
    wc->bssid = tmp;

    data->networks.push_back(std::move(wc));
  }

  callback.Run(std::move(data));
}

}  // namespace arc
