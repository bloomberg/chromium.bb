// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/bluetooth_socket/bluetooth_socket_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothSocket;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothSocket;
using extensions::Extension;

namespace utils = extension_function_test_utils;
namespace api = extensions::api;

namespace {

class BluetoothSocketApiTest : public ExtensionApiTest {
 public:
  BluetoothSocketApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    empty_extension_ = utils::CreateEmptyExtension();
    SetUpMockAdapter();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    ExtensionApiTest::CleanUpOnMainThread();
  }

  void SetUpMockAdapter() {
    // The browser will clean this up when it is torn down.
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    mock_device1_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d1", "11:12:13:14:15:16",
        true /* paired */, false /* connected */));
    mock_device2_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d2", "21:22:23:24:25:26",
        true /* paired */, false /* connected */));
  }

 protected:
  scoped_refptr<testing::StrictMock<MockBluetoothAdapter> > mock_adapter_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > mock_device1_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > mock_device2_;

 private:
  scoped_refptr<Extension> empty_extension_;
};

// testing::InvokeArgument<N> does not work with base::Callback, fortunately
// gmock makes it simple to create action templates that do for the various
// possible numbers of arguments.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  ::std::tr1::get<k>(args).Run();
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  ::std::tr1::get<k>(args).Run(p0);
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  ::std::tr1::get<k>(args).Run(p0, p1);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BluetoothSocketApiTest, Connect) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Return the right mock device object for the address used by the test,
  // return NULL for the "Device not found" test.
  EXPECT_CALL(*mock_adapter_, GetDevice(mock_device1_->GetAddress()))
      .WillRepeatedly(testing::Return(mock_device1_.get()));
  EXPECT_CALL(*mock_adapter_, GetDevice(std::string("aa:aa:aa:aa:aa:aa")))
      .WillOnce(testing::Return(static_cast<BluetoothDevice*>(NULL)));

  // Return a mock socket object as a successful result to the connect() call.
  BluetoothUUID service_uuid("8e3ad063-db38-4289-aa8f-b30e4223cf40");
  scoped_refptr<testing::StrictMock<MockBluetoothSocket> > mock_socket
      = new testing::StrictMock<MockBluetoothSocket>();
  EXPECT_CALL(*mock_device1_,
              ConnectToService(service_uuid, testing::_, testing::_))
      .WillOnce(InvokeCallbackArgument<1>(mock_socket));

  // Since the socket is unpaused, expect a call to Receive() from the socket
  // dispatcher. Since there is no data, this will not call its callback.
  EXPECT_CALL(*mock_socket, Receive(testing::_, testing::_, testing::_));

  // The test also cleans up by calling Disconnect and Close.
  EXPECT_CALL(*mock_socket, Disconnect(testing::_))
      .WillOnce(InvokeCallbackArgument<0>());
  EXPECT_CALL(*mock_socket, Close());

  // Run the test.
  ExtensionTestMessageListener listener("ready", true);
  scoped_refptr<const Extension> extension(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth_socket/connect")));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
