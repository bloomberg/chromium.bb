// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

// Test double for TetherHostFetcher.
class FakeTetherHostFetcher : public TetherHostFetcher {
 public:
  FakeTetherHostFetcher(std::vector<cryptauth::RemoteDevice> tether_hosts,
                        bool synchronously_reply_with_results);
  FakeTetherHostFetcher(bool synchronously_reply_with_results);
  ~FakeTetherHostFetcher() override;

  void SetTetherHosts(const std::vector<cryptauth::RemoteDevice> tether_hosts);

  // If |sychronously_reply_with_results| is false, calling this method will
  // actually invoke the callbacks.
  void InvokePendingCallbacks();

  // TetherHostFetcher:
  void FetchAllTetherHosts(
      const TetherHostFetcher::TetherHostListCallback& callback) override;
  void FetchTetherHost(
      const std::string& device_id,
      const TetherHostFetcher::TetherHostCallback& callback) override;

 private:
  std::vector<cryptauth::RemoteDevice> tether_hosts_;
  bool synchronously_reply_with_results_;

  DISALLOW_COPY_AND_ASSIGN(FakeTetherHostFetcher);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_
