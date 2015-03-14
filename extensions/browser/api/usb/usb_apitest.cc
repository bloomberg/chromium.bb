// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/usb/usb_api.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using content::BrowserThread;
using device::UsbConfigDescriptor;
using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbEndpointDirection;
using device::UsbInterfaceDescriptor;
using device::UsbService;
using device::UsbTransferCallback;

namespace extensions {

namespace {

ACTION_TEMPLATE(InvokeUsbTransferCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  net::IOBuffer* io_buffer = new net::IOBuffer(1);
  memset(io_buffer->data(), 0, 1);  // Avoid uninitialized reads.
  ::std::tr1::get<k>(args).Run(p1, io_buffer, 1);
}

void RequestUsbAccess(int interface_id,
                      const base::Callback<void(bool success)>& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
}

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDevicesChanged() override {
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
      prompt()->GrantDevicePermission(i);
      devices.push_back(prompt()->GetDevice(i));
      if (!prompt()->multiple()) {
        break;
      }
    }
    delegate()->OnUsbDevicesChosen(devices);
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  scoped_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return make_scoped_ptr(new TestDevicePermissionsPrompt(web_contents));
  }
};

class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  MockUsbDeviceHandle() : UsbDeviceHandle() {}

  MOCK_METHOD0(Close, void());

  MOCK_METHOD10(ControlTransfer,
                void(UsbEndpointDirection direction,
                     TransferRequestType request_type,
                     TransferRecipient recipient,
                     uint8 request,
                     uint16 value,
                     uint16 index,
                     net::IOBuffer* buffer,
                     size_t length,
                     unsigned int timeout,
                     const UsbTransferCallback& callback));

  MOCK_METHOD6(BulkTransfer,
               void(UsbEndpointDirection direction,
                    uint8 endpoint,
                    net::IOBuffer* buffer,
                    size_t length,
                    unsigned int timeout,
                    const UsbTransferCallback& callback));

  MOCK_METHOD6(InterruptTransfer,
               void(UsbEndpointDirection direction,
                    uint8 endpoint,
                    net::IOBuffer* buffer,
                    size_t length,
                    unsigned int timeout,
                    const UsbTransferCallback& callback));

  MOCK_METHOD8(IsochronousTransfer,
               void(UsbEndpointDirection direction,
                    uint8 endpoint,
                    net::IOBuffer* buffer,
                    size_t length,
                    unsigned int packets,
                    unsigned int packet_length,
                    unsigned int timeout,
                    const UsbTransferCallback& callback));

  MOCK_METHOD0(ResetDevice, bool());
  MOCK_METHOD2(GetStringDescriptor, bool(uint8_t, base::string16*));
  MOCK_METHOD1(SetConfiguration, bool(int));
  MOCK_METHOD1(ClaimInterface, bool(int interface_number));
  MOCK_METHOD1(ReleaseInterface, bool(int interface_number));
  MOCK_METHOD2(SetInterfaceAlternateSetting,
               bool(int interface_number, int alternate_setting));

  virtual scoped_refptr<UsbDevice> GetDevice() const override {
    return device_;
  }

  void set_device(UsbDevice* device) { device_ = device; }

 protected:
  UsbDevice* device_;

  virtual ~MockUsbDeviceHandle() {}
};

class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice(uint16 vendor_id, uint16 product_id, uint32 unique_id)
      : UsbDevice(vendor_id, product_id, unique_id) {}

  MOCK_METHOD2(RequestUsbAccess, void(int, const base::Callback<void(bool)>&));
  MOCK_METHOD0(Open, scoped_refptr<UsbDeviceHandle>());
  MOCK_METHOD1(Close, bool(scoped_refptr<UsbDeviceHandle>));
  MOCK_METHOD0(GetConfiguration, const device::UsbConfigDescriptor*());
  MOCK_METHOD1(GetManufacturer, bool(base::string16*));
  MOCK_METHOD1(GetProduct, bool(base::string16*));
  MOCK_METHOD1(GetSerialNumber, bool(base::string16*));

 private:
  virtual ~MockUsbDevice() {}
};

class MockUsbService : public UsbService {
 public:
  explicit MockUsbService(scoped_refptr<UsbDevice> device) : device_(device) {}

  // Public wrapper around the protected base class method.
  void NotifyDeviceAdded(scoped_refptr<UsbDevice> device) {
    UsbService::NotifyDeviceAdded(device);
  }

  // Public wrapper around the protected base class method.
  void NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
    UsbService::NotifyDeviceRemoved(device);
  }

 protected:
  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override {
    EXPECT_EQ(unique_id, 0U);
    return device_;
  }

  void GetDevices(std::vector<scoped_refptr<UsbDevice>>* devices) override {
    STLClearObject(devices);
    devices->push_back(device_);
  }

  scoped_refptr<UsbDevice> device_;
};

class UsbApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    mock_device_ = new MockUsbDevice(0, 0, 0);
    EXPECT_CALL(*mock_device_.get(), GetManufacturer(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_device_.get(), GetProduct(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_device_.get(), GetSerialNumber(_))
        .WillRepeatedly(Return(false));

    mock_device_handle_ = new MockUsbDeviceHandle();
    mock_device_handle_->set_device(mock_device_.get());
    EXPECT_CALL(*mock_device_.get(), RequestUsbAccess(_, _))
        .WillRepeatedly(Invoke(RequestUsbAccess));
    EXPECT_CALL(*mock_device_.get(), Open())
        .WillRepeatedly(Return(mock_device_handle_));

    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
                                    base::Bind(&UsbApiTest::SetUpService, this),
                                    run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetUpService() {
    mock_service_ = new MockUsbService(mock_device_);
    UsbService::SetInstanceForTest(mock_service_);
  }

  void AddTestDevices() {
    scoped_refptr<MockUsbDevice> device(new MockUsbDevice(0x18D1, 0x58F0, 1));
    EXPECT_CALL(*device.get(), GetSerialNumber(_))
        .WillRepeatedly(Return(false));
    mock_service_->NotifyDeviceAdded(device);

    device = new MockUsbDevice(0x18D1, 0x58F1, 2);
    EXPECT_CALL(*device.get(), GetSerialNumber(_))
        .WillRepeatedly(Return(false));
    mock_service_->NotifyDeviceAdded(device);
  }

 protected:
  scoped_refptr<MockUsbDeviceHandle> mock_device_handle_;
  scoped_refptr<MockUsbDevice> mock_device_;
  MockUsbService* mock_service_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UsbApiTest, DeviceHandling) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(2);
  ASSERT_TRUE(RunAppTest("api_test/usb/device_handling"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ResetDevice) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(2);
  EXPECT_CALL(*mock_device_handle_.get(), ResetDevice())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_device_handle_.get(),
              InterruptTransfer(device::USB_DIRECTION_OUTBOUND, 2, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  ASSERT_TRUE(RunAppTest("api_test/usb/reset_device"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, SetConfiguration) {
  UsbConfigDescriptor config_descriptor;
  EXPECT_CALL(*mock_device_handle_.get(), SetConfiguration(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(1);
  EXPECT_CALL(*mock_device_.get(), GetConfiguration())
      .WillOnce(Return(nullptr))
      .WillOnce(Return(&config_descriptor));
  ASSERT_TRUE(RunAppTest("api_test/usb/set_configuration"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ListInterfaces) {
  UsbConfigDescriptor config_descriptor;
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(1);
  EXPECT_CALL(*mock_device_.get(), GetConfiguration())
      .WillOnce(Return(&config_descriptor));
  ASSERT_TRUE(RunAppTest("api_test/usb/list_interfaces"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferEvent) {
  EXPECT_CALL(*mock_device_handle_.get(),
              ControlTransfer(device::USB_DIRECTION_OUTBOUND,
                              UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE,
                              1,
                              2,
                              3,
                              _,
                              1,
                              _,
                              _))
      .WillOnce(InvokeUsbTransferCallback<9>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              BulkTransfer(device::USB_DIRECTION_OUTBOUND, 1, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              InterruptTransfer(device::USB_DIRECTION_OUTBOUND, 2, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(
      *mock_device_handle_.get(),
      IsochronousTransfer(device::USB_DIRECTION_OUTBOUND, 3, _, 1, 1, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<7>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_event"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ZeroLengthTransfer) {
  EXPECT_CALL(*mock_device_handle_.get(), BulkTransfer(_, _, _, 0, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/zero_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailure) {
  EXPECT_CALL(*mock_device_handle_.get(), BulkTransfer(_, _, _, _, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_COMPLETED))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_ERROR))
      .WillOnce(InvokeUsbTransferCallback<5>(device::USB_TRANSFER_TIMEOUT));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_failure"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidLengthTransfer) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidTimeout) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_timeout"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&UsbApiTest::AddTestDevices, base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MockUsbService::NotifyDeviceRemoved,
                 base::Unretained(mock_service_), mock_device_),
      run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener ready_listener("opened_device", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(1);

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/usb/get_user_selected_devices"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MockUsbService::NotifyDeviceRemoved,
                 base::Unretained(mock_service_), mock_device_),
      run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

}  // namespace extensions
