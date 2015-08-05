// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/usb/device_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "device/devices_app/usb/type_converters.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "net/base/io_buffer.h"

namespace device {
namespace usb {

namespace {

template <typename... Args>
void CallMojoCallback(const mojo::Callback<void(Args...)>& callback,
                      Args... args) {
  callback.Run(args...);
}

// Generic wrapper to convert a Mojo callback to something we can rebind and
// pass around. This is only usable for callbacks with no move-only arguments.
template <typename... Args>
base::Callback<void(Args...)> WrapMojoCallback(
    const mojo::Callback<void(Args...)>& callback) {
  return base::Bind(&CallMojoCallback<Args...>, callback);
}

scoped_refptr<net::IOBuffer> CreateTransferBuffer(size_t size) {
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(
      std::max(static_cast<size_t>(1u), static_cast<size_t>(size)));
  return buffer;
}

}  // namespace

DeviceImpl::DeviceImpl(scoped_refptr<UsbDeviceHandle> device_handle,
                       mojo::InterfaceRequest<Device> request)
    : binding_(this, request.Pass()),
      device_handle_(device_handle),
      weak_factory_(this) {
}

DeviceImpl::~DeviceImpl() {
  CloseHandle();
}

void DeviceImpl::CloseHandle() {
  if (device_handle_)
    device_handle_->Close();
  device_handle_ = nullptr;
}

void DeviceImpl::OnTransferIn(const MojoTransferInCallback& callback,
                              UsbTransferStatus status,
                              scoped_refptr<net::IOBuffer> buffer,
                              size_t buffer_size) {
  mojo::Array<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): We should change UsbDeviceHandle to use a
    // std::vector<uint8_t> instead of net::IOBuffer. Then we could move
    // instead of copy.
    std::vector<uint8_t> bytes(buffer_size);
    std::copy(buffer->data(), buffer->data() + buffer_size, bytes.begin());
    data.Swap(&bytes);
  }
  callback.Run(mojo::ConvertTo<TransferStatus>(status), data.Pass());
}

void DeviceImpl::OnTransferOut(const MojoTransferOutCallback& callback,
                               UsbTransferStatus status,
                               scoped_refptr<net::IOBuffer> buffer,
                               size_t buffer_size) {
  callback.Run(mojo::ConvertTo<TransferStatus>(status));
}

void DeviceImpl::OnIsochronousTransferIn(
    const IsochronousTransferInCallback& callback,
    uint32_t packet_size,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
  size_t num_packets = buffer_size / packet_size;
  mojo::Array<mojo::Array<uint8_t>> packets(num_packets);
  if (buffer) {
    for (size_t i = 0; i < num_packets; ++i) {
      size_t packet_index = i * packet_size;
      std::vector<uint8_t> bytes(packet_size);
      std::copy(buffer->data() + packet_index,
                buffer->data() + packet_index + packet_size, bytes.begin());
      packets[i].Swap(&bytes);
    }
  }
  callback.Run(mojo::ConvertTo<TransferStatus>(status), packets.Pass());
}

void DeviceImpl::OnIsochronousTransferOut(
    const MojoTransferOutCallback& callback,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
  callback.Run(mojo::ConvertTo<TransferStatus>(status));
}

void DeviceImpl::Close(const CloseCallback& callback) {
  CloseHandle();
  callback.Run();
}

void DeviceImpl::GetDeviceInfo(const GetDeviceInfoCallback& callback) {
  if (!device_handle_) {
    callback.Run(DeviceInfoPtr());
    return;
  }

  // TODO(rockot/reillyg): Support more than just the current configuration.
  // Also, converting configuration info should be done in the TypeConverter,
  // but GetConfiguration() is non-const so we do it here for now.
  DeviceInfoPtr info = DeviceInfo::From(*device_handle_->GetDevice());
  const UsbConfigDescriptor* config =
      device_handle_->GetDevice()->GetActiveConfiguration();
  info->configurations = mojo::Array<ConfigurationInfoPtr>::New(0);
  if (config)
    info->configurations.push_back(ConfigurationInfo::From(*config));
  callback.Run(info.Pass());
}

void DeviceImpl::SetConfiguration(uint8_t value,
                                  const SetConfigurationCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->SetConfiguration(value, WrapMojoCallback(callback));
}

void DeviceImpl::ClaimInterface(uint8_t interface_number,
                                const ClaimInterfaceCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ClaimInterface(interface_number, WrapMojoCallback(callback));
}

void DeviceImpl::ReleaseInterface(uint8_t interface_number,
                                  const ReleaseInterfaceCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  callback.Run(device_handle_->ReleaseInterface(interface_number));
}

void DeviceImpl::SetInterfaceAlternateSetting(
    uint8_t interface_number,
    uint8_t alternate_setting,
    const SetInterfaceAlternateSettingCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->SetInterfaceAlternateSetting(
      interface_number, alternate_setting, WrapMojoCallback(callback));
}

void DeviceImpl::Reset(const ResetCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ResetDevice(WrapMojoCallback(callback));
}

void DeviceImpl::ControlTransferIn(ControlTransferParamsPtr params,
                                   uint32_t length,
                                   uint32_t timeout,
                                   const ControlTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR, mojo::Array<uint8_t>());
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
  device_handle_->ControlTransfer(
      USB_DIRECTION_INBOUND,
      mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
      mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
      params->request, params->value, params->index, buffer, length, timeout,
      base::Bind(&DeviceImpl::OnTransferIn, weak_factory_.GetWeakPtr(),
                 callback));
}

void DeviceImpl::ControlTransferOut(
    ControlTransferParamsPtr params,
    mojo::Array<uint8_t> data,
    uint32_t timeout,
    const ControlTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  const std::vector<uint8_t>& storage = data.storage();
  std::copy(storage.begin(), storage.end(), buffer->data());
  device_handle_->ControlTransfer(
      USB_DIRECTION_OUTBOUND,
      mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
      mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
      params->request, params->value, params->index, buffer, data.size(),
      timeout, base::Bind(&DeviceImpl::OnTransferOut,
                          weak_factory_.GetWeakPtr(), callback));
}

void DeviceImpl::BulkTransferIn(uint8_t endpoint_number,
                                uint32_t length,
                                uint32_t timeout,
                                const BulkTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR, mojo::Array<uint8_t>());
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
  device_handle_->BulkTransfer(
      USB_DIRECTION_INBOUND, endpoint_number, buffer, length, timeout,
      base::Bind(&DeviceImpl::OnTransferIn, weak_factory_.GetWeakPtr(),
                 callback));
}

void DeviceImpl::BulkTransferOut(uint8_t endpoint_number,
                                 mojo::Array<uint8_t> data,
                                 uint32_t timeout,
                                 const BulkTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  const std::vector<uint8_t>& storage = data.storage();
  std::copy(storage.begin(), storage.end(), buffer->data());
  device_handle_->BulkTransfer(
      USB_DIRECTION_OUTBOUND, endpoint_number, buffer, data.size(), timeout,
      base::Bind(&DeviceImpl::OnTransferOut, weak_factory_.GetWeakPtr(),
                 callback));
}

void DeviceImpl::InterruptTransferIn(
    uint8_t endpoint_number,
    uint32_t length,
    uint32_t timeout,
    const InterruptTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR, mojo::Array<uint8_t>());
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
  device_handle_->InterruptTransfer(
      USB_DIRECTION_INBOUND, endpoint_number, buffer, length, timeout,
      base::Bind(&DeviceImpl::OnTransferIn, weak_factory_.GetWeakPtr(),
                 callback));
}

void DeviceImpl::InterruptTransferOut(
    uint8_t endpoint_number,
    mojo::Array<uint8_t> data,
    uint32_t timeout,
    const InterruptTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  const std::vector<uint8_t>& storage = data.storage();
  std::copy(storage.begin(), storage.end(), buffer->data());
  device_handle_->InterruptTransfer(
      USB_DIRECTION_OUTBOUND, endpoint_number, buffer, data.size(), timeout,
      base::Bind(&DeviceImpl::OnTransferOut, weak_factory_.GetWeakPtr(),
                 callback));
}

void DeviceImpl::IsochronousTransferIn(
    uint8_t endpoint_number,
    uint32_t num_packets,
    uint32_t packet_length,
    uint32_t timeout,
    const IsochronousTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR, mojo::Array<mojo::Array<uint8_t>>());
    return;
  }

  size_t transfer_size = static_cast<size_t>(num_packets) * packet_length;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(transfer_size);
  device_handle_->IsochronousTransfer(
      USB_DIRECTION_INBOUND, endpoint_number, buffer, transfer_size,
      num_packets, packet_length, timeout,
      base::Bind(&DeviceImpl::OnIsochronousTransferIn,
                 weak_factory_.GetWeakPtr(), callback, packet_length));
}

void DeviceImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    mojo::Array<mojo::Array<uint8_t>> packets,
    uint32_t timeout,
    const IsochronousTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TRANSFER_STATUS_ERROR);
    return;
  }

  uint32_t packet_size = 0;
  for (size_t i = 0; i < packets.size(); ++i) {
    packet_size =
        std::max(packet_size, static_cast<uint32_t>(packets[i].size()));
  }
  size_t transfer_size = packet_size * packets.size();
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(transfer_size);
  memset(buffer->data(), 0, transfer_size);
  for (size_t i = 0; i < packets.size(); ++i) {
    uint8_t* packet =
        reinterpret_cast<uint8_t*>(&buffer->data()[i * packet_size]);
    DCHECK_LE(packets[i].size(), static_cast<size_t>(packet_size));
    memcpy(packet, packets[i].storage().data(), packets[i].size());
  }
  device_handle_->IsochronousTransfer(
      USB_DIRECTION_OUTBOUND, endpoint_number, buffer, transfer_size,
      static_cast<uint32_t>(packets.size()), packet_size, timeout,
      base::Bind(&DeviceImpl::OnIsochronousTransferOut,
                 weak_factory_.GetWeakPtr(), callback));
}

}  // namespace usb
}  // namespace device
