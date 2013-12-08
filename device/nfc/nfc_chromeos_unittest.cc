// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "device/nfc/nfc_adapter_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::NfcAdapter;

namespace chromeos {

namespace {

class TestObserver : public NfcAdapter::Observer {
 public:
  TestObserver(scoped_refptr<NfcAdapter> adapter)
      : present_changed_count_(0),
        powered_changed_count_(0),
        adapter_(adapter) {
  }

  virtual ~TestObserver() {}

  // NfcAdapter::Observer override.
  virtual void AdapterPresentChanged(NfcAdapter* adapter,
                                     bool present) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    present_changed_count_++;
  }

  // NfcAdapter::Observer override.
  virtual void AdapterPoweredChanged(NfcAdapter* adapter,
                                     bool powered) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    powered_changed_count_++;
  }

  int present_changed_count_;
  int powered_changed_count_;
  scoped_refptr<NfcAdapter> adapter_;
};

}  // namespace

class NfcChromeOSTest : public testing::Test {
 public:
  virtual void SetUp() {
    DBusThreadManager::InitializeWithStub();
    fake_nfc_adapter_client_ = static_cast<FakeNfcAdapterClient*>(
        DBusThreadManager::Get()->GetNfcAdapterClient());
    SetAdapter();
    message_loop_.RunUntilIdle();

    success_callback_count_ = 0;
    error_callback_count_ = 0;
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  // Assigns a new instance of NfcAdapterChromeOS to |adapter_|.
  void SetAdapter() {
    adapter_ = new NfcAdapterChromeOS();
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Generic callbacks for success and error.
  void SuccessCallback() {
    success_callback_count_++;
  }

  void ErrorCallback() {
    error_callback_count_++;
  }

 protected:
  // Fields for storing the number of times SuccessCallback and ErrorCallback
  // have been called.
  int success_callback_count_;
  int error_callback_count_;

  // A message loop to emulate asynchronous behavior.
  base::MessageLoop message_loop_;

  // The NfcAdapter instance under test.
  scoped_refptr<NfcAdapter> adapter_;

  // The fake D-Bus client instances used for testing.
  FakeNfcAdapterClient* fake_nfc_adapter_client_;
};

TEST_F(NfcChromeOSTest, PresentChanged) {
  EXPECT_TRUE(adapter_->IsPresent());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Remove all adapters.
  fake_nfc_adapter_client_->SetAdapterPresent(false);
  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(adapter_->IsPresent());

  // Add two adapters.
  fake_nfc_adapter_client_->SetAdapterPresent(true);
  fake_nfc_adapter_client_->SetSecondAdapterPresent(true);
  EXPECT_EQ(2, observer.present_changed_count_);
  EXPECT_TRUE(adapter_->IsPresent());

  // Remove the first adapter. Adapter  should update to the second one.
  fake_nfc_adapter_client_->SetAdapterPresent(false);
  EXPECT_EQ(4, observer.present_changed_count_);
  EXPECT_TRUE(adapter_->IsPresent());

  fake_nfc_adapter_client_->SetSecondAdapterPresent(false);
  EXPECT_EQ(5, observer.present_changed_count_);
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(NfcChromeOSTest, SetPowered) {
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  EXPECT_FALSE(adapter_->IsPowered());

  // SetPowered(false), while not powered.
  adapter_->SetPowered(
      false,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  // SetPowered(true).
  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  // SetPowered(true), while powered.
  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);

  // SetPowered(false).
  adapter_->SetPowered(
      false,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count_);
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
}

TEST_F(NfcChromeOSTest, PresentChangedWhilePowered) {
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsPresent());

  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(adapter_->IsPowered());

  fake_nfc_adapter_client_->SetAdapterPresent(false);
  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_EQ(2, observer.powered_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsPresent());
}

}  // namespace chromeos
