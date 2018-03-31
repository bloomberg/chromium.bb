// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DEVICE_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DEVICE_H_

#include <vector>

#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockRemoteDevice : public RemoteDevice {
 public:
  MockRemoteDevice();

  void Connect(StatusCallback cb) override {}
  MOCK_METHOD0(ConnectSync, bool());
  void Disconnect(StatusCallback cb) override {}
  MOCK_METHOD0(DisconnectSync, bool());
  void ReadRemoteRssi(RssiCallback cb) override {}
  void RequestMtu(int mtu, StatusCallback cb) override {}
  void ConnectionParameterUpdate(int min_interval,
                                 int max_interval,
                                 int latency,
                                 int timeout,
                                 StatusCallback cb) override {}
  void DiscoverServices(DiscoverServicesCb cb) override {}
  MOCK_METHOD0(IsConnected, bool());
  MOCK_METHOD0(GetMtu, int());
  void GetServices(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)> cb)
      override {}
  MOCK_METHOD0(GetServicesSync, std::vector<scoped_refptr<RemoteService>>());
  void GetServiceByUuid(
      const bluetooth_v2_shlib::Uuid& uuid,
      base::OnceCallback<void(scoped_refptr<RemoteService>)> cb) override {}
  MOCK_METHOD1(
      GetServiceByUuidSync,
      scoped_refptr<RemoteService>(const bluetooth_v2_shlib::Uuid& uuid));
  MOCK_CONST_METHOD0(addr, const bluetooth_v2_shlib::Addr&());

 private:
  ~MockRemoteDevice();
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DEVICE_H_
