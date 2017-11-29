// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_tether_host_fetcher.h"

#include "base/memory/ptr_util.h"

namespace chromeos {

namespace tether {

FakeTetherHostFetcher::FakeTetherHostFetcher(
    std::vector<cryptauth::RemoteDevice> tether_hosts,
    bool synchronously_reply_with_results)
    : TetherHostFetcher(nullptr),
      tether_hosts_(tether_hosts),
      synchronously_reply_with_results_(synchronously_reply_with_results) {}

FakeTetherHostFetcher::FakeTetherHostFetcher(
    bool synchronously_reply_with_results)
    : FakeTetherHostFetcher(std::vector<cryptauth::RemoteDevice>(),
                            synchronously_reply_with_results) {}

FakeTetherHostFetcher::~FakeTetherHostFetcher() = default;

void FakeTetherHostFetcher::SetTetherHosts(
    const std::vector<cryptauth::RemoteDevice> tether_hosts) {
  tether_hosts_ = tether_hosts;
}

void FakeTetherHostFetcher::InvokePendingCallbacks() {
  OnRemoteDevicesLoaded(tether_hosts_);
}

void FakeTetherHostFetcher::FetchAllTetherHosts(
    const TetherHostFetcher::TetherHostListCallback& callback) {
  requests_.push_back(TetherHostFetchRequest(callback));
  if (synchronously_reply_with_results_) {
    InvokePendingCallbacks();
  }
}

void FakeTetherHostFetcher::FetchTetherHost(
    const std::string& device_id,
    const TetherHostFetcher::TetherHostCallback& callback) {
  requests_.push_back(TetherHostFetchRequest(device_id, callback));
  if (synchronously_reply_with_results_) {
    InvokePendingCallbacks();
  }
}

}  // namespace tether

}  // namespace chromeos
