// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_MOCK_U2F_DISCOVERY_H_
#define DEVICE_U2F_MOCK_U2F_DISCOVERY_H_

#include <memory>
#include <string>

#include "device/u2f/u2f_device.h"
#include "device/u2f/u2f_discovery.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockU2fDiscovery : public U2fDiscovery {
 public:
  class MockDelegate : public Delegate {
   public:
    MockDelegate();
    ~MockDelegate();

    MOCK_METHOD1(OnStarted, void(bool));
    MOCK_METHOD1(OnStopped, void(bool));
    MOCK_METHOD1(OnDeviceAddedStr, void(const std::string&));
    MOCK_METHOD1(OnDeviceRemovedStr, void(const std::string&));

    // Workaround for GMock's inability to handle move-only types.
    // Only the device id is forwarded to avoid implementing a custom matcher.
    void OnDeviceAdded(std::unique_ptr<U2fDevice> device) override;
    void OnDeviceRemoved(base::StringPiece device_id) override;

    base::WeakPtr<MockDelegate> GetWeakPtr();

   private:
    base::WeakPtrFactory<MockDelegate> weak_factory_;
  };

  MockU2fDiscovery();
  ~MockU2fDiscovery() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());

  Delegate* delegate() { return delegate_.get(); }

  void AddDevice(std::unique_ptr<U2fDevice> device);
  void RemoveDevice(base::StringPiece device_id);

  void StartSuccess();
  void StartFailure();

  void StartSuccessAsync();
  void StartFailureAsync();
};

}  // namespace device

#endif  // DEVICE_U2F_MOCK_U2F_DISCOVERY_H_
