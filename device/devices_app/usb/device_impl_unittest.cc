// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "device/devices_app/usb/device_impl.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

using ::testing::Invoke;
using ::testing::_;

namespace device {
namespace usb {

namespace {

class ConfigBuilder {
 public:
  ConfigBuilder(uint8_t value) { config_.configuration_value = value; }

  ConfigBuilder& AddInterface(uint8_t interface_number,
                              uint8_t alternate_setting,
                              uint8_t class_code,
                              uint8_t subclass_code,
                              uint8_t protocol_code) {
    UsbInterfaceDescriptor interface;
    interface.interface_number = interface_number;
    interface.alternate_setting = alternate_setting;
    interface.interface_class = class_code;
    interface.interface_subclass = subclass_code;
    interface.interface_protocol = protocol_code;
    config_.interfaces.push_back(interface);
    return *this;
  }

  const UsbConfigDescriptor& config() const { return config_; }

 private:
  UsbConfigDescriptor config_;
};

void ExpectDeviceInfoAndThen(const std::string& guid,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& manufacturer,
                             const std::string& product,
                             const std::string& serial_number,
                             const base::Closure& continuation,
                             DeviceInfoPtr device_info) {
  EXPECT_EQ(guid, device_info->guid);
  EXPECT_EQ(vendor_id, device_info->vendor_id);
  EXPECT_EQ(product_id, device_info->product_id);
  EXPECT_EQ(manufacturer, device_info->manufacturer);
  EXPECT_EQ(product, device_info->product);
  EXPECT_EQ(serial_number, device_info->serial_number);
  continuation.Run();
}

void ExpectResultAndThen(bool expected_result,
                         const base::Closure& continuation,
                         bool actual_result) {
  EXPECT_EQ(expected_result, actual_result);
  continuation.Run();
}

void ExpectTransferInAndThen(TransferStatus expected_status,
                             const std::vector<uint8_t>& expected_bytes,
                             const base::Closure& continuation,
                             TransferStatus actual_status,
                             mojo::Array<uint8_t> actual_bytes) {
  EXPECT_EQ(expected_status, actual_status);
  ASSERT_EQ(expected_bytes.size(), actual_bytes.size());
  for (size_t i = 0; i < actual_bytes.size(); ++i) {
    EXPECT_EQ(expected_bytes[i], actual_bytes[i])
        << "Contents differ at index: " << i;
  }
  continuation.Run();
}

void ExpectPacketsAndThen(
    TransferStatus expected_status,
    const std::vector<std::vector<uint8_t>>& expected_packets,
    const base::Closure& continuation,
    TransferStatus actual_status,
    mojo::Array<mojo::Array<uint8_t>> actual_packets) {
  EXPECT_EQ(expected_status, actual_status);
  ASSERT_EQ(expected_packets.size(), actual_packets.size());
  for (size_t i = 0; i < expected_packets.size(); ++i) {
    EXPECT_EQ(expected_packets[i].size(), actual_packets[i].size())
        << "Packet sizes differ at index: " << i;
    for (size_t j = 0; j < expected_packets[i].size(); ++j) {
      EXPECT_EQ(expected_packets[i][j], actual_packets[i][j])
          << "Contents of packet " << i << " differ at index " << j;
    }
  }
  continuation.Run();
}

void ExpectTransferStatusAndThen(TransferStatus expected_status,
                                 const base::Closure& continuation,
                                 TransferStatus actual_status) {
  EXPECT_EQ(expected_status, actual_status);
  continuation.Run();
}

class USBDeviceImplTest : public testing::Test {
 public:
  USBDeviceImplTest()
      : message_loop_(new base::MessageLoop),
        is_device_open_(false),
        allow_reset_(false),
        current_config_(0) {}

  ~USBDeviceImplTest() override {}

 protected:
  MockUsbDevice& mock_device() { return *mock_device_.get(); }
  bool is_device_open() const { return is_device_open_; }
  MockUsbDeviceHandle& mock_handle() { return *mock_handle_.get(); }

  void set_allow_reset(bool allow_reset) { allow_reset_ = allow_reset; }

  // Creates a mock device and binds a Device proxy to a Device service impl
  // wrapping the mock device.
  DevicePtr GetMockDeviceProxy(uint16_t vendor_id,
                               uint16_t product_id,
                               const std::string& manufacturer,
                               const std::string& product,
                               const std::string& serial) {
    is_device_open_ = true;

    mock_device_ =
        new MockUsbDevice(vendor_id, product_id, manufacturer, product, serial);
    mock_handle_ = new MockUsbDeviceHandle(mock_device_.get());

    DevicePtr proxy;
    new DeviceImpl(mock_handle_, mojo::GetProxy(&proxy));

    // Set up mock handle calls to respond based on mock device configs
    // established by the test.
    ON_CALL(mock_handle(), Close())
        .WillByDefault(Invoke(this, &USBDeviceImplTest::CloseMockHandle));
    ON_CALL(mock_handle(), SetConfiguration(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::SetConfiguration));
    ON_CALL(mock_handle(), ClaimInterface(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ClaimInterface));
    ON_CALL(mock_handle(), ReleaseInterface(_))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ReleaseInterface));
    ON_CALL(mock_handle(), SetInterfaceAlternateSetting(_, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::SetInterfaceAlternateSetting));
    ON_CALL(mock_handle(), ResetDevice(_))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ResetDevice));
    ON_CALL(mock_handle(), ControlTransfer(_, _, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ControlTransfer));
    ON_CALL(mock_handle(), BulkTransfer(_, _, _, _, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::BulkOrInterruptTransfer));
    ON_CALL(mock_handle(), InterruptTransfer(_, _, _, _, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::BulkOrInterruptTransfer));
    ON_CALL(mock_handle(), IsochronousTransfer(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::IsochronousTransfer));

    return proxy.Pass();
  }

  DevicePtr GetMockDeviceProxy() {
    return GetMockDeviceProxy(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  }

  void AddMockConfig(const ConfigBuilder& builder) {
    const UsbConfigDescriptor& config = builder.config();
    DCHECK(!ContainsKey(mock_configs_, config.configuration_value));
    mock_configs_[config.configuration_value] = config;
  }

  void AddMockInboundData(const std::vector<uint8_t>& data) {
    mock_inbound_data_.push(data);
  }

  void AddMockOutboundData(const std::vector<uint8_t>& data) {
    mock_outbound_data_.push(data);
  }

 private:
  void CloseMockHandle() {
    EXPECT_TRUE(is_device_open_);
    is_device_open_ = false;
  }

  void SetConfiguration(uint8_t value,
                        const UsbDeviceHandle::ResultCallback& callback) {
    if (mock_configs_.find(value) != mock_configs_.end()) {
      current_config_ = value;
      callback.Run(true);
    } else {
      callback.Run(false);
    }
  }

  void ClaimInterface(uint8_t interface_number,
                      const UsbDeviceHandle::ResultCallback& callback) {
    for (const auto& config : mock_configs_) {
      for (const auto& interface : config.second.interfaces) {
        if (interface.interface_number == interface_number) {
          claimed_interfaces_.insert(interface_number);
          callback.Run(true);
          return;
        }
      }
    }
    callback.Run(false);
  }

  bool ReleaseInterface(uint8_t interface_number) {
    if (ContainsKey(claimed_interfaces_, interface_number)) {
      claimed_interfaces_.erase(interface_number);
      return true;
    }
    return false;
  }

  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      const UsbDeviceHandle::ResultCallback& callback) {
    for (const auto& config : mock_configs_) {
      for (const auto& interface : config.second.interfaces) {
        if (interface.interface_number == interface_number &&
            interface.alternate_setting == alternate_setting) {
          callback.Run(true);
          return;
        }
      }
    }
    callback.Run(false);
  }

  void ResetDevice(const UsbDeviceHandle::ResultCallback& callback) {
    callback.Run(allow_reset_);
  }

  void InboundTransfer(const UsbDeviceHandle::TransferCallback& callback) {
    ASSERT_GE(mock_inbound_data_.size(), 1u);
    const std::vector<uint8_t>& bytes = mock_inbound_data_.front();
    size_t length = bytes.size();
    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(length);
    std::copy(bytes.begin(), bytes.end(), buffer->data());
    mock_inbound_data_.pop();
    callback.Run(USB_TRANSFER_COMPLETED, buffer, length);
  }

  void OutboundTransfer(scoped_refptr<net::IOBuffer> buffer,
                        size_t length,
                        const UsbDeviceHandle::TransferCallback& callback) {
    ASSERT_GE(mock_outbound_data_.size(), 1u);
    const std::vector<uint8_t>& bytes = mock_outbound_data_.front();
    ASSERT_EQ(bytes.size(), length);
    for (size_t i = 0; i < length; ++i) {
      EXPECT_EQ(bytes[i], buffer->data()[i])
          << "Contents differ at index: " << i;
    }
    mock_outbound_data_.pop();
    callback.Run(USB_TRANSFER_COMPLETED, buffer, length);
  }

  void ControlTransfer(UsbEndpointDirection direction,
                       UsbDeviceHandle::TransferRequestType request_type,
                       UsbDeviceHandle::TransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const UsbDeviceHandle::TransferCallback& callback) {
    if (direction == USB_DIRECTION_INBOUND)
      InboundTransfer(callback);
    else
      OutboundTransfer(buffer, length, callback);
  }

  void BulkOrInterruptTransfer(
      UsbEndpointDirection direction,
      uint8_t endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      size_t length,
      unsigned int timeout,
      const UsbDeviceHandle::TransferCallback& callback) {
    if (direction == USB_DIRECTION_INBOUND)
      InboundTransfer(callback);
    else
      OutboundTransfer(buffer, length, callback);
  }

  void IsochronousTransfer(UsbEndpointDirection direction,
                           uint8_t endpoint,
                           scoped_refptr<net::IOBuffer> buffer,
                           size_t length,
                           unsigned int packets,
                           unsigned int packet_length,
                           unsigned int timeout,
                           const UsbDeviceHandle::TransferCallback& callback) {
    if (direction == USB_DIRECTION_INBOUND)
      InboundTransfer(callback);
    else
      OutboundTransfer(buffer, length, callback);
  }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<MockUsbDevice> mock_device_;
  scoped_refptr<MockUsbDeviceHandle> mock_handle_;
  bool is_device_open_;
  bool allow_reset_;

  std::map<uint8_t, UsbConfigDescriptor> mock_configs_;
  uint8_t current_config_;

  std::queue<std::vector<uint8_t>> mock_inbound_data_;
  std::queue<std::vector<uint8_t>> mock_outbound_data_;

  std::set<uint8_t> claimed_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(USBDeviceImplTest);
};

}  // namespace

TEST_F(USBDeviceImplTest, Close) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_TRUE(is_device_open());

  EXPECT_CALL(mock_handle(), Close());

  base::RunLoop loop;
  device->Close(loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(is_device_open());
}

// Test that the information returned via the Device::GetDeviceInfo matches that
// of the underlying device.
TEST_F(USBDeviceImplTest, GetDeviceInfo) {
  DevicePtr device =
      GetMockDeviceProxy(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  EXPECT_CALL(mock_device(), GetActiveConfiguration());

  base::RunLoop loop;
  device->GetDeviceInfo(base::Bind(&ExpectDeviceInfoAndThen,
                                   mock_device().guid(), 0x1234, 0x5678, "ACME",
                                   "Frobinator", "ABCDEF", loop.QuitClosure()));
  loop.Run();

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetInvalidConfiguration) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_handle(), SetConfiguration(42, _));

  // SetConfiguration should fail because 42 is not a valid mock configuration.
  base::RunLoop loop;
  device->SetConfiguration(
      42, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
  loop.Run();

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetValidConfiguration) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_handle(), SetConfiguration(42, _));

  AddMockConfig(ConfigBuilder(42));

  // SetConfiguration should succeed because 42 is a valid mock configuration.
  base::RunLoop loop;
  device->SetConfiguration(
      42, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
  loop.Run();

  EXPECT_CALL(mock_handle(), Close());
}

// Verify that the result of Reset() reflects the underlying UsbDeviceHandle's
// ResetDevice() result.
TEST_F(USBDeviceImplTest, Reset) {
  DevicePtr device = GetMockDeviceProxy();

  EXPECT_CALL(mock_handle(), ResetDevice(_));

  set_allow_reset(true);

  {
    base::RunLoop loop;
    device->Reset(base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ResetDevice(_));

  set_allow_reset(false);

  {
    base::RunLoop loop;
    device->Reset(base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, ClaimAndReleaseInterface) {
  DevicePtr device = GetMockDeviceProxy();

  // Now add a mock interface #1.
  AddMockConfig(ConfigBuilder(0).AddInterface(1, 0, 1, 2, 3));

  EXPECT_CALL(mock_handle(), ClaimInterface(2, _));

  {
    // Try to claim an invalid interface and expect failure.
    base::RunLoop loop;
    device->ClaimInterface(
        2, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ClaimInterface(1, _));

  {
    base::RunLoop loop;
    device->ClaimInterface(
        1, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ReleaseInterface(2));

  {
    // Releasing a non-existent interface should fail.
    base::RunLoop loop;
    device->ReleaseInterface(
        2, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ReleaseInterface(1));

  {
    // Now this should release the claimed interface and close the handle.
    base::RunLoop loop;
    device->ReleaseInterface(
        1, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetInterfaceAlternateSetting) {
  DevicePtr device = GetMockDeviceProxy();

  AddMockConfig(ConfigBuilder(0)
                    .AddInterface(1, 0, 1, 2, 3)
                    .AddInterface(1, 42, 1, 2, 3)
                    .AddInterface(2, 0, 1, 2, 3));

  EXPECT_CALL(mock_handle(), SetInterfaceAlternateSetting(1, 42, _));

  {
    base::RunLoop loop;
    device->SetInterfaceAlternateSetting(
        1, 42, base::Bind(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetInterfaceAlternateSetting(1, 100, _));

  {
    base::RunLoop loop;
    device->SetInterfaceAlternateSetting(
        1, 100, base::Bind(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, ControlTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  std::vector<uint8_t> fake_data;
  fake_data.push_back(41);
  fake_data.push_back(42);
  fake_data.push_back(43);

  AddMockInboundData(fake_data);

  EXPECT_CALL(mock_handle(),
              ControlTransfer(USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE, 5, 6, 7, _, _, 0, _));

  {
    auto params = ControlTransferParams::New();
    params->type = CONTROL_TRANSFER_TYPE_STANDARD;
    params->recipient = CONTROL_TRANSFER_RECIPIENT_DEVICE;
    params->request = 5;
    params->value = 6;
    params->index = 7;
    base::RunLoop loop;
    device->ControlTransferIn(
        params.Pass(), static_cast<uint32_t>(fake_data.size()), 0,
        base::Bind(&ExpectTransferInAndThen, TRANSFER_STATUS_COMPLETED,
                   fake_data, loop.QuitClosure()));
    loop.Run();
  }

  AddMockConfig(ConfigBuilder(0).AddInterface(7, 0, 1, 2, 3));
  AddMockOutboundData(fake_data);

  EXPECT_CALL(mock_handle(),
              ControlTransfer(USB_DIRECTION_OUTBOUND, UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::INTERFACE, 5, 6, 7, _, _, 0, _));

  {
    auto params = ControlTransferParams::New();
    params->type = CONTROL_TRANSFER_TYPE_STANDARD;
    params->recipient = CONTROL_TRANSFER_RECIPIENT_INTERFACE;
    params->request = 5;
    params->value = 6;
    params->index = 7;
    base::RunLoop loop;
    device->ControlTransferOut(
        params.Pass(), mojo::Array<uint8_t>::From(fake_data), 0,
        base::Bind(&ExpectTransferStatusAndThen, TRANSFER_STATUS_COMPLETED,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, BulkTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  std::string message1 = "say hello please";
  std::vector<uint8_t> fake_outbound_data(message1.size());
  std::copy(message1.begin(), message1.end(), fake_outbound_data.begin());

  std::string message2 = "hello world!";
  std::vector<uint8_t> fake_inbound_data(message2.size());
  std::copy(message2.begin(), message2.end(), fake_inbound_data.begin());

  AddMockConfig(ConfigBuilder(0).AddInterface(7, 0, 1, 2, 3));
  AddMockOutboundData(fake_outbound_data);
  AddMockInboundData(fake_inbound_data);

  EXPECT_CALL(mock_handle(), BulkTransfer(USB_DIRECTION_OUTBOUND, 0, _,
                                          fake_outbound_data.size(), 0, _));

  {
    base::RunLoop loop;
    device->BulkTransferOut(
        0, mojo::Array<uint8_t>::From(fake_outbound_data), 0,
        base::Bind(&ExpectTransferStatusAndThen, TRANSFER_STATUS_COMPLETED,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), BulkTransfer(USB_DIRECTION_INBOUND, 0, _,
                                          fake_inbound_data.size(), 0, _));

  {
    base::RunLoop loop;
    device->BulkTransferIn(
        0, static_cast<uint32_t>(fake_inbound_data.size()), 0,
        base::Bind(&ExpectTransferInAndThen, TRANSFER_STATUS_COMPLETED,
                   fake_inbound_data, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, InterruptTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  std::string message1 = "say hello please";
  std::vector<uint8_t> fake_outbound_data(message1.size());
  std::copy(message1.begin(), message1.end(), fake_outbound_data.begin());

  std::string message2 = "hello world!";
  std::vector<uint8_t> fake_inbound_data(message2.size());
  std::copy(message2.begin(), message2.end(), fake_inbound_data.begin());

  AddMockConfig(ConfigBuilder(0).AddInterface(7, 0, 1, 2, 3));
  AddMockOutboundData(fake_outbound_data);
  AddMockInboundData(fake_inbound_data);

  EXPECT_CALL(mock_handle(),
              InterruptTransfer(USB_DIRECTION_OUTBOUND, 0, _,
                                fake_outbound_data.size(), 0, _));

  {
    base::RunLoop loop;
    device->InterruptTransferOut(
        0, mojo::Array<uint8_t>::From(fake_outbound_data), 0,
        base::Bind(&ExpectTransferStatusAndThen, TRANSFER_STATUS_COMPLETED,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), InterruptTransfer(USB_DIRECTION_INBOUND, 0, _,
                                               fake_inbound_data.size(), 0, _));

  {
    base::RunLoop loop;
    device->InterruptTransferIn(
        0, static_cast<uint32_t>(fake_inbound_data.size()), 0,
        base::Bind(&ExpectTransferInAndThen, TRANSFER_STATUS_COMPLETED,
                   fake_inbound_data, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, IsochronousTransfer) {
  DevicePtr device = GetMockDeviceProxy();

  std::string outbound_packet_data = "aaaaaaaabbbbbbbbccccccccdddddddd";
  std::vector<uint8_t> fake_outbound_packets(outbound_packet_data.size());
  std::copy(outbound_packet_data.begin(), outbound_packet_data.end(),
            fake_outbound_packets.begin());

  std::string inbound_packet_data = "ddddddddccccccccbbbbbbbbaaaaaaaa";
  std::vector<uint8_t> fake_inbound_packets(inbound_packet_data.size());
  std::copy(inbound_packet_data.begin(), inbound_packet_data.end(),
            fake_inbound_packets.begin());

  AddMockConfig(ConfigBuilder(0).AddInterface(7, 0, 1, 2, 3));
  AddMockOutboundData(fake_outbound_packets);
  AddMockInboundData(fake_inbound_packets);

  EXPECT_CALL(mock_handle(),
              IsochronousTransfer(USB_DIRECTION_OUTBOUND, 0, _,
                                  fake_outbound_packets.size(), 4, 8, 0, _));

  {
    base::RunLoop loop;
    mojo::Array<mojo::Array<uint8_t>> packets =
        mojo::Array<mojo::Array<uint8_t>>::New(4);
    for (size_t i = 0; i < 4; ++i) {
      std::vector<uint8_t> bytes(8);
      std::copy(outbound_packet_data.begin() + i * 8,
                outbound_packet_data.begin() + i * 8 + 8, bytes.begin());
      packets[i].Swap(&bytes);
    }
    device->IsochronousTransferOut(
        0, packets.Pass(), 0,
        base::Bind(&ExpectTransferStatusAndThen, TRANSFER_STATUS_COMPLETED,
                   loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(),
              IsochronousTransfer(USB_DIRECTION_INBOUND, 0, _,
                                  fake_inbound_packets.size(), 4, 8, 0, _));

  {
    base::RunLoop loop;
    std::vector<std::vector<uint8_t>> packets(4);
    for (size_t i = 0; i < 4; ++i) {
      packets[i].resize(8);
      std::copy(inbound_packet_data.begin() + i * 8,
                inbound_packet_data.begin() + i * 8 + 8, packets[i].begin());
    }
    device->IsochronousTransferIn(
        0, 4, 8, 0, base::Bind(&ExpectPacketsAndThen, TRANSFER_STATUS_COMPLETED,
                               packets, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

}  // namespace usb
}  // namespace device
