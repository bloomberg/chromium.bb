// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_usb_device.h"

#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_socket.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "device/base/device_client.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using device::UsbConfigDescriptor;
using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbInterfaceDescriptor;
using device::UsbEndpointDescriptor;
using device::UsbService;
using device::UsbTransferStatus;

namespace {

const size_t kHeaderSize = 24;

const int kAdbClass = 0xff;
const int kAdbSubclass = 0x42;
const int kAdbProtocol = 0x1;

const int kUsbTimeout = 0;

const uint32_t kMaxPayload = 4096;
const uint32_t kVersion = 0x01000000;

static const char kHostConnectMessage[] = "host::";

using content::BrowserThread;

typedef std::vector<scoped_refptr<UsbDevice> > UsbDevices;
typedef std::set<scoped_refptr<UsbDevice> > UsbDeviceSet;

// Stores android wrappers around claimed usb devices on caller thread.
base::LazyInstance<std::vector<AndroidUsbDevice*> >::Leaky g_devices =
    LAZY_INSTANCE_INITIALIZER;

bool IsAndroidInterface(const UsbInterfaceDescriptor& interface) {
  if (interface.alternate_setting != 0 ||
      interface.interface_class != kAdbClass ||
      interface.interface_subclass != kAdbSubclass ||
      interface.interface_protocol != kAdbProtocol ||
      interface.endpoints.size() != 2) {
    return false;
  }
  return true;
}

void CountAndroidDevices(const base::Callback<void(int)>& callback,
                         const UsbDevices& devices) {
  int device_count = 0;
  for (const scoped_refptr<UsbDevice>& device : devices) {
    const UsbConfigDescriptor* config = device->active_configuration();
    if (config) {
      for (const UsbInterfaceDescriptor& iface : config->interfaces) {
        if (IsAndroidInterface(iface)) {
          ++device_count;
        }
      }
    }
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, device_count));
}

uint32_t Checksum(const std::string& data) {
  unsigned char* x = (unsigned char*)data.data();
  int count = data.length();
  uint32_t sum = 0;
  while (count-- > 0)
    sum += *x++;
  return sum;
}

void DumpMessage(bool outgoing, const char* data, size_t length) {
#if 0
  std::string result = "";
  if (length == kHeaderSize) {
    for (size_t i = 0; i < 24; ++i) {
      result += base::StringPrintf("%02x",
          data[i] > 0 ? data[i] : (data[i] + 0x100) & 0xFF);
      if ((i + 1) % 4 == 0)
        result += " ";
    }
    for (size_t i = 0; i < 24; ++i) {
      if (data[i] >= 0x20 && data[i] <= 0x7E)
        result += data[i];
      else
        result += ".";
    }
  } else {
    result = base::StringPrintf("%d: ", (int)length);
    for (size_t i = 0; i < length; ++i) {
      if (data[i] >= 0x20 && data[i] <= 0x7E)
        result += data[i];
      else
        result += ".";
    }
  }
  LOG(ERROR) << (outgoing ? "[out] " : "[ in] ") << result;
#endif  // 0
}

void CloseDevice(scoped_refptr<UsbDeviceHandle> usb_device,
                 bool release_successful) {
  usb_device->Close();
}

void ReleaseInterface(scoped_refptr<UsbDeviceHandle> usb_device,
                      int interface_id) {
  usb_device->ReleaseInterface(interface_id,
                               base::Bind(&CloseDevice, usb_device));
}

void RespondOnCallerThread(const AndroidUsbDevicesCallback& callback,
                           AndroidUsbDevices* new_devices) {
  std::unique_ptr<AndroidUsbDevices> devices(new_devices);

  // Add raw pointers to the newly claimed devices.
  for (const scoped_refptr<AndroidUsbDevice>& device : *devices) {
    g_devices.Get().push_back(device.get());
  }

  // Return all claimed devices.
  AndroidUsbDevices result(g_devices.Get().begin(), g_devices.Get().end());
  callback.Run(result);
}

void RespondOnUIThread(
    const AndroidUsbDevicesCallback& callback,
    AndroidUsbDevices* devices,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  caller_task_runner->PostTask(
      FROM_HERE, base::Bind(&RespondOnCallerThread, callback, devices));
}

void CreateDeviceOnInterfaceClaimed(AndroidUsbDevices* devices,
                                    crypto::RSAPrivateKey* rsa_key,
                                    scoped_refptr<UsbDeviceHandle> usb_handle,
                                    int inbound_address,
                                    int outbound_address,
                                    int zero_mask,
                                    int interface_number,
                                    const base::Closure& barrier,
                                    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (success) {
    devices->push_back(new AndroidUsbDevice(
        rsa_key, usb_handle,
        base::UTF16ToASCII(usb_handle->GetDevice()->serial_number()),
        inbound_address, outbound_address, zero_mask, interface_number));
  } else {
    usb_handle->Close();
  }
  barrier.Run();
}

void OnDeviceOpened(AndroidUsbDevices* devices,
                    crypto::RSAPrivateKey* rsa_key,
                    int inbound_address,
                    int outbound_address,
                    int zero_mask,
                    int interface_number,
                    const base::Closure& barrier,
                    scoped_refptr<UsbDeviceHandle> usb_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (usb_handle.get()) {
    usb_handle->ClaimInterface(
        interface_number,
        base::Bind(&CreateDeviceOnInterfaceClaimed, devices, rsa_key,
                   usb_handle, inbound_address, outbound_address, zero_mask,
                   interface_number, barrier));
  } else {
    barrier.Run();
  }
}

void OpenAndroidDevice(AndroidUsbDevices* devices,
                       crypto::RSAPrivateKey* rsa_key,
                       const base::Closure& barrier,
                       scoped_refptr<UsbDevice> device,
                       int interface_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (device->serial_number().empty()) {
    barrier.Run();
    return;
  }

  const UsbConfigDescriptor* config = device->active_configuration();
  if (!config) {
    barrier.Run();
    return;
  }

  const UsbInterfaceDescriptor& interface = config->interfaces[interface_id];
  int inbound_address = 0;
  int outbound_address = 0;
  int zero_mask = 0;

  for (const UsbEndpointDescriptor& endpoint : interface.endpoints) {
    if (endpoint.transfer_type != device::USB_TRANSFER_BULK)
      continue;
    if (endpoint.direction == device::USB_DIRECTION_INBOUND)
      inbound_address = endpoint.address;
    else
      outbound_address = endpoint.address;
    zero_mask = endpoint.maximum_packet_size - 1;
  }

  if (inbound_address == 0 || outbound_address == 0) {
    barrier.Run();
    return;
  }

  device->Open(base::Bind(&OnDeviceOpened, devices, rsa_key, inbound_address,
                          outbound_address, zero_mask,
                          interface.interface_number, barrier));
}

void OpenAndroidDevices(
    crypto::RSAPrivateKey* rsa_key,
    const AndroidUsbDevicesCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    const UsbDevices& usb_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Add new devices.
  AndroidUsbDevices* devices = new AndroidUsbDevices();
  base::Closure barrier = base::BarrierClosure(
      usb_devices.size(),
      base::Bind(&RespondOnUIThread, callback, devices, caller_task_runner));

  for (const scoped_refptr<UsbDevice>& device : usb_devices) {
    const UsbConfigDescriptor* config = device->active_configuration();
    if (!config) {
      barrier.Run();
      continue;
    }
    bool has_android_interface = false;
    for (size_t j = 0; j < config->interfaces.size(); ++j) {
      if (!IsAndroidInterface(config->interfaces[j])) {
        continue;
      }

      OpenAndroidDevice(devices, rsa_key, barrier, device, j);
      has_android_interface = true;
      break;
    }
    if (!has_android_interface) {
      barrier.Run();
    }
  }
}

void EnumerateOnUIThread(
    crypto::RSAPrivateKey* rsa_key,
    const AndroidUsbDevicesCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (service == NULL) {
    caller_task_runner->PostTask(FROM_HERE,
                                 base::Bind(callback, AndroidUsbDevices()));
  } else {
    service->GetDevices(
        base::Bind(&OpenAndroidDevices, rsa_key, callback, caller_task_runner));
  }
}

}  // namespace

AdbMessage::AdbMessage(uint32_t command,
                       uint32_t arg0,
                       uint32_t arg1,
                       const std::string& body)
    : command(command), arg0(arg0), arg1(arg1), body(body) {}

AdbMessage::~AdbMessage() {
}

// static
void AndroidUsbDevice::CountDevices(const base::Callback<void(int)>& callback) {
  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (service != NULL) {
    service->GetDevices(base::Bind(&CountAndroidDevices, callback));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, 0));
  }
}

// static
void AndroidUsbDevice::Enumerate(crypto::RSAPrivateKey* rsa_key,
                                 const AndroidUsbDevicesCallback& callback) {
  // Collect devices with closed handles.
  for (AndroidUsbDevice* device : g_devices.Get()) {
    if (device->usb_handle_.get()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(&AndroidUsbDevice::TerminateIfReleased,
                                         device, device->usb_handle_));
    }
  }

  // Then look for the new devices.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&EnumerateOnUIThread, rsa_key, callback,
                                     base::ThreadTaskRunnerHandle::Get()));
}

AndroidUsbDevice::AndroidUsbDevice(crypto::RSAPrivateKey* rsa_key,
                                   scoped_refptr<UsbDeviceHandle> usb_device,
                                   const std::string& serial,
                                   int inbound_address,
                                   int outbound_address,
                                   int zero_mask,
                                   int interface_id)
    : rsa_key_(rsa_key->Copy()),
      usb_handle_(usb_device),
      serial_(serial),
      inbound_address_(inbound_address),
      outbound_address_(outbound_address),
      zero_mask_(zero_mask),
      interface_id_(interface_id),
      is_connected_(false),
      signature_sent_(false),
      last_socket_id_(256),
      weak_factory_(this) {
}

void AndroidUsbDevice::InitOnCallerThread() {
  if (task_runner_)
    return;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  Queue(base::MakeUnique<AdbMessage>(AdbMessage::kCommandCNXN, kVersion,
                                     kMaxPayload, kHostConnectMessage));
  ReadHeader();
}

net::StreamSocket* AndroidUsbDevice::CreateSocket(const std::string& command) {
  if (!usb_handle_.get())
    return NULL;

  uint32_t socket_id = ++last_socket_id_;
  sockets_[socket_id] = new AndroidUsbSocket(this, socket_id, command,
      base::Bind(&AndroidUsbDevice::SocketDeleted, this, socket_id));
  return sockets_[socket_id];
}

void AndroidUsbDevice::Send(uint32_t command,
                            uint32_t arg0,
                            uint32_t arg1,
                            const std::string& body) {
  std::unique_ptr<AdbMessage> message(
      new AdbMessage(command, arg0, arg1, body));
  // Delay open request if not yet connected.
  if (!is_connected_) {
    pending_messages_.push_back(message.release());
    return;
  }
  Queue(std::move(message));
}

AndroidUsbDevice::~AndroidUsbDevice() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Terminate();
}

void AndroidUsbDevice::Queue(std::unique_ptr<AdbMessage> message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Queue header.
  std::vector<uint32_t> header;
  header.push_back(message->command);
  header.push_back(message->arg0);
  header.push_back(message->arg1);
  bool append_zero = true;
  if (message->body.empty())
    append_zero = false;
  if (message->command == AdbMessage::kCommandAUTH &&
      message->arg0 == AdbMessage::kAuthSignature)
    append_zero = false;
  if (message->command == AdbMessage::kCommandWRTE)
    append_zero = false;

  size_t body_length = message->body.length() + (append_zero ? 1 : 0);
  header.push_back(body_length);
  header.push_back(Checksum(message->body));
  header.push_back(message->command ^ 0xffffffff);
  scoped_refptr<net::IOBufferWithSize> header_buffer =
      new net::IOBufferWithSize(kHeaderSize);
  memcpy(header_buffer.get()->data(), &header[0], kHeaderSize);
  outgoing_queue_.push(header_buffer);

  // Queue body.
  if (!message->body.empty()) {
    scoped_refptr<net::IOBufferWithSize> body_buffer =
        new net::IOBufferWithSize(body_length);
    memcpy(body_buffer->data(), message->body.data(), message->body.length());
    if (append_zero)
      body_buffer->data()[body_length - 1] = 0;
    outgoing_queue_.push(body_buffer);
    if (zero_mask_ && (body_length & zero_mask_) == 0) {
      // Send a zero length packet.
      outgoing_queue_.push(new net::IOBufferWithSize(0));
    }
  }
  ProcessOutgoing();
}

void AndroidUsbDevice::ProcessOutgoing() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (outgoing_queue_.empty() || !usb_handle_.get())
    return;

  BulkMessage message = outgoing_queue_.front();
  outgoing_queue_.pop();
  DumpMessage(true, message->data(), message->size());

  usb_handle_->GenericTransfer(
      device::USB_DIRECTION_OUTBOUND, outbound_address_, message,
      message->size(), kUsbTimeout,
      base::Bind(&AndroidUsbDevice::OutgoingMessageSent,
                 weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::OutgoingMessageSent(UsbTransferStatus status,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           size_t result) {
  if (status != device::USB_TRANSFER_COMPLETED) {
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&AndroidUsbDevice::ProcessOutgoing, this));
}

void AndroidUsbDevice::ReadHeader() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!usb_handle_.get()) {
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kHeaderSize);
  usb_handle_->GenericTransfer(
      device::USB_DIRECTION_INBOUND, inbound_address_, buffer, kHeaderSize,
      kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseHeader, weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::ParseHeader(UsbTransferStatus status,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   size_t result) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == device::USB_TRANSFER_TIMEOUT) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&AndroidUsbDevice::ReadHeader, this));
    return;
  }

  if (status != device::USB_TRANSFER_COMPLETED || result != kHeaderSize) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer->data(), result);
  std::vector<uint32_t> header(6);
  memcpy(&header[0], buffer->data(), result);
  std::unique_ptr<AdbMessage> message(
      new AdbMessage(header[0], header[1], header[2], ""));
  uint32_t data_length = header[3];
  uint32_t data_check = header[4];
  uint32_t magic = header[5];
  if ((message->command ^ 0xffffffff) != magic) {
    TransferError(device::USB_TRANSFER_ERROR);
    return;
  }

  if (data_length == 0) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&AndroidUsbDevice::HandleIncoming, this,
                                      base::Passed(&message)));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&AndroidUsbDevice::ReadBody, this,
                              base::Passed(&message), data_length, data_check));
  }
}

void AndroidUsbDevice::ReadBody(std::unique_ptr<AdbMessage> message,
                                uint32_t data_length,
                                uint32_t data_check) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!usb_handle_.get()) {
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      new net::IOBuffer(static_cast<size_t>(data_length));
  usb_handle_->GenericTransfer(
      device::USB_DIRECTION_INBOUND, inbound_address_, buffer, data_length,
      kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseBody, weak_factory_.GetWeakPtr(),
                 base::Passed(&message), data_length, data_check));
}

void AndroidUsbDevice::ParseBody(std::unique_ptr<AdbMessage> message,
                                 uint32_t data_length,
                                 uint32_t data_check,
                                 UsbTransferStatus status,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 size_t result) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == device::USB_TRANSFER_TIMEOUT) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&AndroidUsbDevice::ReadBody, this,
                              base::Passed(&message), data_length, data_check));
    return;
  }

  if (status != device::USB_TRANSFER_COMPLETED ||
      static_cast<uint32_t>(result) != data_length) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer->data(), data_length);
  message->body = std::string(buffer->data(), result);
  if (Checksum(message->body) != data_check) {
    TransferError(device::USB_TRANSFER_ERROR);
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&AndroidUsbDevice::HandleIncoming, this,
                                    base::Passed(&message)));
}

void AndroidUsbDevice::HandleIncoming(std::unique_ptr<AdbMessage> message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  switch (message->command) {
    case AdbMessage::kCommandAUTH:
      {
      DCHECK_EQ(message->arg0, static_cast<uint32_t>(AdbMessage::kAuthToken));
        if (signature_sent_) {
          Queue(base::MakeUnique<AdbMessage>(
              AdbMessage::kCommandAUTH, AdbMessage::kAuthRSAPublicKey, 0,
              AndroidRSAPublicKey(rsa_key_.get())));
        } else {
          signature_sent_ = true;
          std::string signature = AndroidRSASign(rsa_key_.get(), message->body);
          if (!signature.empty()) {
            Queue(base::MakeUnique<AdbMessage>(AdbMessage::kCommandAUTH,
                                               AdbMessage::kAuthSignature, 0,
                                               signature));
          } else {
            Queue(base::MakeUnique<AdbMessage>(
                AdbMessage::kCommandAUTH, AdbMessage::kAuthRSAPublicKey, 0,
                AndroidRSAPublicKey(rsa_key_.get())));
          }
        }
      }
      break;
    case AdbMessage::kCommandCNXN:
      {
        is_connected_ = true;
        PendingMessages pending;
        pending.swap(pending_messages_);
        for (PendingMessages::iterator it = pending.begin();
             it != pending.end(); ++it) {
          Queue(base::WrapUnique(*it));
        }
      }
      break;
    case AdbMessage::kCommandOKAY:
    case AdbMessage::kCommandWRTE:
    case AdbMessage::kCommandCLSE:
      {
        AndroidUsbSockets::iterator it = sockets_.find(message->arg1);
        if (it != sockets_.end())
          it->second->HandleIncoming(std::move(message));
      }
      break;
    default:
      break;
  }
  ReadHeader();
}

void AndroidUsbDevice::TransferError(UsbTransferStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  Terminate();
}

void AndroidUsbDevice::TerminateIfReleased(
    scoped_refptr<UsbDeviceHandle> usb_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (usb_handle->GetDevice().get()) {
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&AndroidUsbDevice::Terminate, this));
}

void AndroidUsbDevice::Terminate() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::vector<AndroidUsbDevice*>::iterator it =
      std::find(g_devices.Get().begin(), g_devices.Get().end(), this);
  if (it != g_devices.Get().end())
    g_devices.Get().erase(it);

  if (!usb_handle_.get())
    return;

  // Make sure we zero-out handle so that closing connections did not open
  // new connections.
  scoped_refptr<UsbDeviceHandle> usb_handle = usb_handle_;
  usb_handle_ = NULL;

  // Iterate over copy.
  AndroidUsbSockets sockets(sockets_);
  for (AndroidUsbSockets::iterator it = sockets.begin();
       it != sockets.end(); ++it) {
    it->second->Terminated(true);
  }
  DCHECK(sockets_.empty());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ReleaseInterface, usb_handle, interface_id_));
}

void AndroidUsbDevice::SocketDeleted(uint32_t socket_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  sockets_.erase(socket_id);
}
