// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/devices_app/usb/device_impl.h"

#include <stddef.h>

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

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

using MojoTransferInCallback =
    mojo::Callback<void(TransferStatus, mojo::Array<uint8_t>)>;

using MojoTransferOutCallback = mojo::Callback<void(TransferStatus)>;

template <typename... Args>
void CallMojoCallback(scoped_ptr<mojo::Callback<void(Args...)>> callback,
                      Args... args) {
  callback->Run(args...);
}

// Generic wrapper to convert a Mojo callback to something we can rebind and
// pass around. This is only usable for callbacks with no move-only arguments.
template <typename... Args>
base::Callback<void(Args...)> WrapMojoCallback(
    const mojo::Callback<void(Args...)>& callback) {
  // mojo::Callback is not thread safe. By wrapping |callback| in a scoped_ptr
  // we guarantee that it will be freed when CallMojoCallback is run and not
  // retained until the base::Callback is destroyed, which could happen on any
  // thread. This pattern is also used below in places where this generic
  // wrapper is not used.
  auto callback_ptr =
      make_scoped_ptr(new mojo::Callback<void(Args...)>(callback));
  return base::Bind(&CallMojoCallback<Args...>, base::Passed(&callback_ptr));
}

void OnPermissionCheckComplete(
    const base::Callback<void(bool)>& callback,
    const base::Callback<void(const base::Callback<void(bool)>&)>& action,
    bool allowed) {
  if (allowed)
    action.Run(callback);
  else
    callback.Run(false);
}

scoped_refptr<net::IOBuffer> CreateTransferBuffer(size_t size) {
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(
      std::max(static_cast<size_t>(1u), static_cast<size_t>(size)));
  return buffer;
}

void OnTransferIn(scoped_ptr<MojoTransferInCallback> callback,
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
  callback->Run(mojo::ConvertTo<TransferStatus>(status), std::move(data));
}

void OnControlTransferInPermissionCheckComplete(
    scoped_refptr<UsbDeviceHandle> device_handle,
    ControlTransferParamsPtr params,
    int length,
    int timeout,
    scoped_ptr<Device::ControlTransferInCallback> callback,
    bool allowed) {
  if (allowed) {
    scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
    device_handle->ControlTransfer(
        USB_DIRECTION_INBOUND,
        mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
        mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
        params->request, params->value, params->index, buffer, length, timeout,
        base::Bind(&OnTransferIn, base::Passed(&callback)));
  } else {
    mojo::Array<uint8_t> data;
    callback->Run(TransferStatus::PERMISSION_DENIED, std::move(data));
  }
}

void OnTransferOut(scoped_ptr<MojoTransferOutCallback> callback,
                   UsbTransferStatus status,
                   scoped_refptr<net::IOBuffer> buffer,
                   size_t buffer_size) {
  callback->Run(mojo::ConvertTo<TransferStatus>(status));
}

void OnControlTransferOutPermissionCheckComplete(
    scoped_refptr<UsbDeviceHandle> device_handle,
    ControlTransferParamsPtr params,
    mojo::Array<uint8_t> data,
    int timeout,
    scoped_ptr<Device::ControlTransferOutCallback> callback,
    bool allowed) {
  if (allowed) {
    scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
    const std::vector<uint8_t>& storage = data.storage();
    std::copy(storage.begin(), storage.end(), buffer->data());
    device_handle->ControlTransfer(
        USB_DIRECTION_OUTBOUND,
        mojo::ConvertTo<UsbDeviceHandle::TransferRequestType>(params->type),
        mojo::ConvertTo<UsbDeviceHandle::TransferRecipient>(params->recipient),
        params->request, params->value, params->index, buffer, data.size(),
        timeout, base::Bind(&OnTransferOut, base::Passed(&callback)));
  } else {
    callback->Run(TransferStatus::PERMISSION_DENIED);
  }
}

mojo::Array<IsochronousPacketPtr> BuildIsochronousPacketArray(
    mojo::Array<uint32_t> packet_lengths,
    TransferStatus status) {
  mojo::Array<IsochronousPacketPtr> packets(packet_lengths.size());
  for (size_t i = 0; i < packet_lengths.size(); ++i) {
    packets[i] = IsochronousPacket::New();
    packets[i]->length = packet_lengths[i];
    packets[i]->status = status;
  }
  return packets;
}

void OnIsochronousTransferIn(
    scoped_ptr<Device::IsochronousTransferInCallback> callback,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  mojo::Array<uint8_t> data;
  if (buffer) {
    // TODO(rockot/reillyg): We should change UsbDeviceHandle to use a
    // std::vector<uint8_t> instead of net::IOBuffer. Then we could move
    // instead of copy.
    uint32_t buffer_size =
        std::accumulate(packets.begin(), packets.end(), 0u,
                        [](const uint32_t& a,
                           const UsbDeviceHandle::IsochronousPacket& packet) {
                          return a + packet.length;
                        });
    std::vector<uint8_t> bytes(buffer_size);
    std::copy(buffer->data(), buffer->data() + buffer_size, bytes.begin());
    data.Swap(&bytes);
  }
  callback->Run(std::move(data),
                mojo::Array<IsochronousPacketPtr>::From(packets));
}

void OnIsochronousTransferOut(
    scoped_ptr<Device::IsochronousTransferOutCallback> callback,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  callback->Run(mojo::Array<IsochronousPacketPtr>::From(packets));
}

}  // namespace

DeviceImpl::DeviceImpl(scoped_refptr<UsbDevice> device,
                       PermissionProviderPtr permission_provider,
                       mojo::InterfaceRequest<Device> request)
    : binding_(this, std::move(request)),
      device_(device),
      permission_provider_(std::move(permission_provider)),
      weak_factory_(this) {
  // This object owns itself and will be destroyed if either the message pipe
  // it is bound to is closed or the PermissionProvider it depends on is
  // unavailable.
  binding_.set_connection_error_handler([this]() { delete this; });
  permission_provider_.set_connection_error_handler([this]() { delete this; });
}

DeviceImpl::~DeviceImpl() {
  CloseHandle();
}

void DeviceImpl::CloseHandle() {
  if (device_handle_)
    device_handle_->Close();
  device_handle_ = nullptr;
}

void DeviceImpl::HasControlTransferPermission(
    ControlTransferRecipient recipient,
    uint16_t index,
    const base::Callback<void(bool)>& callback) {
  DCHECK(device_handle_);
  const UsbConfigDescriptor* config = device_->GetActiveConfiguration();

  if (recipient == ControlTransferRecipient::INTERFACE ||
      recipient == ControlTransferRecipient::ENDPOINT) {
    if (!config) {
      callback.Run(false);
      return;
    }

    uint8_t interface_number = index & 0xff;
    if (recipient == ControlTransferRecipient::ENDPOINT) {
      if (!device_handle_->FindInterfaceByEndpoint(index & 0xff,
                                                   &interface_number)) {
        callback.Run(false);
        return;
      }
    }

    permission_provider_->HasInterfacePermission(
        interface_number, config->configuration_value,
        DeviceInfo::From(*device_), callback);
  } else if (config) {
    permission_provider_->HasConfigurationPermission(
        config->configuration_value, DeviceInfo::From(*device_), callback);
  } else {
    // Client must already have device permission to have gotten this far.
    callback.Run(true);
  }
}

void DeviceImpl::OnOpen(const OpenCallback& callback,
                        scoped_refptr<UsbDeviceHandle> handle) {
  device_handle_ = handle;
  callback.Run(handle ? OpenDeviceError::OK : OpenDeviceError::ACCESS_DENIED);
}

void DeviceImpl::GetDeviceInfo(const GetDeviceInfoCallback& callback) {
  callback.Run(DeviceInfo::From(*device_));
}

void DeviceImpl::GetConfiguration(const GetConfigurationCallback& callback) {
  const UsbConfigDescriptor* config = device_->GetActiveConfiguration();
  callback.Run(config ? config->configuration_value : 0);
}

void DeviceImpl::Open(const OpenCallback& callback) {
  device_->Open(
      base::Bind(&DeviceImpl::OnOpen, weak_factory_.GetWeakPtr(), callback));
}

void DeviceImpl::Close(const CloseCallback& callback) {
  CloseHandle();
  callback.Run();
}

void DeviceImpl::SetConfiguration(uint8_t value,
                                  const SetConfigurationCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  auto set_configuration =
      base::Bind(&UsbDeviceHandle::SetConfiguration, device_handle_, value);
  permission_provider_->HasConfigurationPermission(
      value, DeviceInfo::From(*device_),
      base::Bind(&OnPermissionCheckComplete, WrapMojoCallback(callback),
                 set_configuration));
}

void DeviceImpl::ClaimInterface(uint8_t interface_number,
                                const ClaimInterfaceCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  const UsbConfigDescriptor* config = device_->GetActiveConfiguration();
  if (!config) {
    callback.Run(false);
    return;
  }

  auto claim_interface = base::Bind(&UsbDeviceHandle::ClaimInterface,
                                    device_handle_, interface_number);
  permission_provider_->HasInterfacePermission(
      interface_number, config->configuration_value, DeviceInfo::From(*device_),
      base::Bind(&OnPermissionCheckComplete, WrapMojoCallback(callback),
                 claim_interface));
}

void DeviceImpl::ReleaseInterface(uint8_t interface_number,
                                  const ReleaseInterfaceCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ReleaseInterface(interface_number,
                                   WrapMojoCallback(callback));
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

void DeviceImpl::ClearHalt(uint8_t endpoint,
                           const ClearHaltCallback& callback) {
  if (!device_handle_) {
    callback.Run(false);
    return;
  }

  device_handle_->ClearHalt(endpoint, WrapMojoCallback(callback));
}

void DeviceImpl::ControlTransferIn(ControlTransferParamsPtr params,
                                   uint32_t length,
                                   uint32_t timeout,
                                   const ControlTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR, mojo::Array<uint8_t>());
    return;
  }

  auto callback_ptr = make_scoped_ptr(new ControlTransferInCallback(callback));
  ControlTransferRecipient recipient = params->recipient;
  uint16_t index = params->index;
  HasControlTransferPermission(
      recipient, index,
      base::Bind(&OnControlTransferInPermissionCheckComplete, device_handle_,
                 base::Passed(&params), length, timeout,
                 base::Passed(&callback_ptr)));
}

void DeviceImpl::ControlTransferOut(
    ControlTransferParamsPtr params,
    mojo::Array<uint8_t> data,
    uint32_t timeout,
    const ControlTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR);
    return;
  }

  auto callback_ptr = make_scoped_ptr(new ControlTransferOutCallback(callback));
  ControlTransferRecipient recipient = params->recipient;
  uint16_t index = params->index;
  HasControlTransferPermission(
      recipient, index,
      base::Bind(&OnControlTransferOutPermissionCheckComplete, device_handle_,
                 base::Passed(&params), base::Passed(&data), timeout,
                 base::Passed(&callback_ptr)));
}

void DeviceImpl::GenericTransferIn(uint8_t endpoint_number,
                                   uint32_t length,
                                   uint32_t timeout,
                                   const GenericTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR, mojo::Array<uint8_t>());
    return;
  }

  auto callback_ptr = make_scoped_ptr(new GenericTransferInCallback(callback));
  uint8_t endpoint_address = endpoint_number | 0x80;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(length);
  device_handle_->GenericTransfer(
      USB_DIRECTION_INBOUND, endpoint_address, buffer, length, timeout,
      base::Bind(&OnTransferIn, base::Passed(&callback_ptr)));
}

void DeviceImpl::GenericTransferOut(
    uint8_t endpoint_number,
    mojo::Array<uint8_t> data,
    uint32_t timeout,
    const GenericTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(TransferStatus::TRANSFER_ERROR);
    return;
  }

  auto callback_ptr = make_scoped_ptr(new GenericTransferOutCallback(callback));
  uint8_t endpoint_address = endpoint_number;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  const std::vector<uint8_t>& storage = data.storage();
  std::copy(storage.begin(), storage.end(), buffer->data());
  device_handle_->GenericTransfer(
      USB_DIRECTION_OUTBOUND, endpoint_address, buffer, data.size(), timeout,
      base::Bind(&OnTransferOut, base::Passed(&callback_ptr)));
}

void DeviceImpl::IsochronousTransferIn(
    uint8_t endpoint_number,
    mojo::Array<uint32_t> packet_lengths,
    uint32_t timeout,
    const IsochronousTransferInCallback& callback) {
  if (!device_handle_) {
    callback.Run(mojo::Array<uint8_t>(),
                 BuildIsochronousPacketArray(std::move(packet_lengths),
                                             TransferStatus::TRANSFER_ERROR));
    return;
  }

  auto callback_ptr =
      make_scoped_ptr(new IsochronousTransferInCallback(callback));
  uint8_t endpoint_address = endpoint_number | 0x80;
  device_handle_->IsochronousTransferIn(
      endpoint_address, packet_lengths.storage(), timeout,
      base::Bind(&OnIsochronousTransferIn, base::Passed(&callback_ptr)));
}

void DeviceImpl::IsochronousTransferOut(
    uint8_t endpoint_number,
    mojo::Array<uint8_t> data,
    mojo::Array<uint32_t> packet_lengths,
    uint32_t timeout,
    const IsochronousTransferOutCallback& callback) {
  if (!device_handle_) {
    callback.Run(BuildIsochronousPacketArray(std::move(packet_lengths),
                                             TransferStatus::TRANSFER_ERROR));
    return;
  }

  auto callback_ptr =
      make_scoped_ptr(new IsochronousTransferOutCallback(callback));
  uint8_t endpoint_address = endpoint_number;
  scoped_refptr<net::IOBuffer> buffer = CreateTransferBuffer(data.size());
  {
    const std::vector<uint8_t>& storage = data.storage();
    std::copy(storage.begin(), storage.end(), buffer->data());
  }
  device_handle_->IsochronousTransferOut(
      endpoint_address, buffer, packet_lengths.storage(), timeout,
      base::Bind(&OnIsochronousTransferOut, base::Passed(&callback_ptr)));
}

}  // namespace usb
}  // namespace device
