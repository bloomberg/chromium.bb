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

  virtual void AddObserver(BluetoothAdapter::Observer* observer) override {
  }

  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) override {

  }

  virtual std::string GetAddress() const override {
    return "";
  }

  virtual std::string GetName() const override {
    return "";
  }

  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override {
  }

  virtual bool IsInitialized() const override {
    return false;
  }

  virtual bool IsPresent() const override {
    return false;
  }

  virtual bool IsPowered() const override {
    return false;
  }

  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
  }

  virtual bool IsDiscoverable() const override {
    return false;
  }

  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
  }

  virtual bool IsDiscovering() const override {
    return false;
  }

  virtual void StartDiscoverySession(
      const DiscoverySessionCallback& callback,
      const ErrorCallback& error_callback) override {
  }

  virtual void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {
  }

  virtual void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {
  }

 protected:
  virtual ~TestBluetoothAdapter() {}

  virtual void AddDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
  }

  virtual void RemoveDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
  }

  virtual void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override {
  }
};

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
  public:
   virtual void RequestPinCode(BluetoothDevice* device) override {}
   virtual void RequestPasskey(BluetoothDevice* device) override {}
   virtual void DisplayPinCode(BluetoothDevice* device,
                               const std::string& pincode) override {}
   virtual void DisplayPasskey(BluetoothDevice* device,
                               uint32 passkey) override {}
   virtual void KeysEntered(BluetoothDevice* device,
                            uint32 entered) override {}
   virtual void ConfirmPasskey(BluetoothDevice* device,
                               uint32 passkey) override {}
   virtual void AuthorizePairing(BluetoothDevice* device) override {}
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
