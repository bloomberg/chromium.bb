// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;

namespace device {

class TestBluetoothAdapter : public BluetoothAdapter {
 public:
  TestBluetoothAdapter() {
  }

  void AddObserver(BluetoothAdapter::Observer* observer) override {}

  void RemoveObserver(BluetoothAdapter::Observer* observer) override {}

  std::string GetAddress() const override { return ""; }

  std::string GetName() const override { return ""; }

  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override {}

  bool IsInitialized() const override { return false; }

  bool IsPresent() const override { return false; }

  bool IsPowered() const override { return false; }

  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override {}

  bool IsDiscoverable() const override { return false; }

  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override {}

  bool IsDiscovering() const override { return false; }

  void DeleteOnCorrectThread() const override { delete this; }

  void StartDiscoverySession(const DiscoverySessionCallback& callback,
                             const ErrorCallback& error_callback) override {}

  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {}

  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {}

  void RegisterAudioSink(
      const BluetoothAudioSink::Options& options,
      const AcquiredCallback& callback,
      const BluetoothAudioSink::ErrorCallback& error_callback) override {}

 protected:
  ~TestBluetoothAdapter() override {}

  void AddDiscoverySession(const base::Closure& callback,
                           const ErrorCallback& error_callback) override {}

  void RemoveDiscoverySession(const base::Closure& callback,
                              const ErrorCallback& error_callback) override {}

  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override {}
};

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  void RequestPinCode(BluetoothDevice* device) override {}
  void RequestPasskey(BluetoothDevice* device) override {}
  void DisplayPinCode(BluetoothDevice* device,
                      const std::string& pincode) override {}
  void DisplayPasskey(BluetoothDevice* device, uint32 passkey) override {}
  void KeysEntered(BluetoothDevice* device, uint32 entered) override {}
  void ConfirmPasskey(BluetoothDevice* device, uint32 passkey) override {}
  void AuthorizePairing(BluetoothDevice* device) override {}
};


TEST(BluetoothAdapterTest, NoDefaultPairingDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there is no registered pairing delegate, NULL is returned.
  EXPECT_TRUE(adapter->DefaultPairingDelegate() == NULL);
}

TEST(BluetoothAdapterTest, OneDefaultPairingDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there is one registered pairing delegate, it is returned.
  TestPairingDelegate delegate;

  adapter->AddPairingDelegate(&delegate,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  EXPECT_EQ(&delegate, adapter->DefaultPairingDelegate());
}

TEST(BluetoothAdapterTest, SamePriorityDelegates) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there are two registered pairing delegates of the same
  // priority, the first one registered is returned.
  TestPairingDelegate delegate1, delegate2;

  adapter->AddPairingDelegate(&delegate1,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter->AddPairingDelegate(&delegate2,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  EXPECT_EQ(&delegate1, adapter->DefaultPairingDelegate());

  // After unregistering the first, the second can be returned.
  adapter->RemovePairingDelegate(&delegate1);

  EXPECT_EQ(&delegate2, adapter->DefaultPairingDelegate());
}

TEST(BluetoothAdapterTest, HighestPriorityDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there are two registered pairing delegates, the one with
  // the highest priority is returned.
  TestPairingDelegate delegate1, delegate2;

  adapter->AddPairingDelegate(&delegate1,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter->AddPairingDelegate(&delegate2,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  EXPECT_EQ(&delegate2, adapter->DefaultPairingDelegate());
}

TEST(BluetoothAdapterTest, UnregisterDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that after unregistering a delegate, NULL is returned.
  TestPairingDelegate delegate;

  adapter->AddPairingDelegate(&delegate,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter->RemovePairingDelegate(&delegate);

  EXPECT_TRUE(adapter->DefaultPairingDelegate() == NULL);
}

}  // namespace device
