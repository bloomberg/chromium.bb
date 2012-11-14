// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "content/public/test/test_browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kAdapterAddress = "bluetooth adapter address";

}  // namespace

namespace device {

class FakeBluetoothAdapterWin : public BluetoothAdapterWin {
 public:
  FakeBluetoothAdapterWin() : BluetoothAdapterWin() {}

  virtual void UpdateAdapterState() {
    address_ = adapter_address_;
  }

  void SetAdapterAddressForTest(const std::string& adapter_address) {
    adapter_address_ = adapter_address;
  }

 private:
  virtual ~FakeBluetoothAdapterWin() {}
  std::string adapter_address_;
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothAdapterWin);
};

class BluetoothAdapterWinTest : public testing::Test {
 public:
  BluetoothAdapterWinTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  }

  virtual void SetUp() {
    adapter_ = new FakeBluetoothAdapterWin();
    adapter_->TrackDefaultAdapter();
  }

  void SetAdapterAddressForTest() {
    adapter_->SetAdapterAddressForTest(kAdapterAddress);
  }

  int GetPollIntervalMs() const {
    return BluetoothAdapterWin::kPollIntervalMs;
  }

 protected:
  scoped_refptr<FakeBluetoothAdapterWin> adapter_;

  // Main message loop for the test.
  MessageLoopForUI loop_;

  // Main thread for the test.
  content::TestBrowserThread ui_thread_;

  // NOTE: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWinTest> weak_ptr_factory_;
};

TEST_F(BluetoothAdapterWinTest, Polling) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &BluetoothAdapterWinTest::SetAdapterAddressForTest,
          weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(GetPollIntervalMs() - 1));
  EXPECT_STRNE(kAdapterAddress, adapter_->address().c_str());
  base::RunLoop run_loop;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(GetPollIntervalMs() + 1));

  run_loop.Run();

  EXPECT_STREQ(kAdapterAddress, adapter_->address().c_str());
}

TEST_F(BluetoothAdapterWinTest, IsPresent) {
  EXPECT_FALSE(adapter_->IsPresent());
  SetAdapterAddressForTest();
  base::RunLoop run_loop;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(GetPollIntervalMs()));
  run_loop.Run();

  EXPECT_TRUE(adapter_->IsPresent());
}

}  // namespace device