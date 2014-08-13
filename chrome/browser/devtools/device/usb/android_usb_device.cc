// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_usb_device.h"

#include <set>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_socket.h"
#include "components/usb_service/usb_device.h"
#include "components/usb_service/usb_interface.h"
#include "components/usb_service/usb_service.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using usb_service::UsbConfigDescriptor;
using usb_service::UsbDevice;
using usb_service::UsbDeviceHandle;
using usb_service::UsbInterfaceAltSettingDescriptor;
using usb_service::UsbInterfaceDescriptor;
using usb_service::UsbEndpointDescriptor;
using usb_service::UsbService;
using usb_service::UsbTransferStatus;

namespace {

const size_t kHeaderSize = 24;

const int kAdbClass = 0xff;
const int kAdbSubclass = 0x42;
const int kAdbProtocol = 0x1;

const int kUsbTimeout = 0;

const uint32 kMaxPayload = 4096;
const uint32 kVersion = 0x01000000;

static const char kHostConnectMessage[] = "host::";

using content::BrowserThread;

typedef std::vector<scoped_refptr<UsbDevice> > UsbDevices;
typedef std::set<scoped_refptr<UsbDevice> > UsbDeviceSet;

// Stores android wrappers around claimed usb devices on caller thread.
base::LazyInstance<std::vector<AndroidUsbDevice*> >::Leaky g_devices =
    LAZY_INSTANCE_INITIALIZER;

bool IsAndroidInterface(
    scoped_refptr<const usb_service::UsbInterfaceDescriptor> interface) {
  if (interface->GetNumAltSettings() == 0)
    return false;

  scoped_refptr<const UsbInterfaceAltSettingDescriptor> idesc =
      interface->GetAltSetting(0);

  if (idesc->GetInterfaceClass() != kAdbClass ||
      idesc->GetInterfaceSubclass() != kAdbSubclass ||
      idesc->GetInterfaceProtocol() != kAdbProtocol ||
      idesc->GetNumEndpoints() != 2) {
    return false;
  }
  return true;
}

scoped_refptr<AndroidUsbDevice> ClaimInterface(
    crypto::RSAPrivateKey* rsa_key,
    scoped_refptr<UsbDeviceHandle> usb_handle,
    scoped_refptr<const UsbInterfaceDescriptor> interface,
    int interface_id) {
  scoped_refptr<const UsbInterfaceAltSettingDescriptor> idesc =
      interface->GetAltSetting(0);

  int inbound_address = 0;
  int outbound_address = 0;
  int zero_mask = 0;

  for (size_t i = 0; i < idesc->GetNumEndpoints(); ++i) {
    scoped_refptr<const UsbEndpointDescriptor> edesc =
        idesc->GetEndpoint(i);
    if (edesc->GetTransferType() != usb_service::USB_TRANSFER_BULK)
      continue;
    if (edesc->GetDirection() == usb_service::USB_DIRECTION_INBOUND)
      inbound_address = edesc->GetAddress();
    else
      outbound_address = edesc->GetAddress();
    zero_mask = edesc->GetMaximumPacketSize() - 1;
  }

  if (inbound_address == 0 || outbound_address == 0)
    return NULL;

  if (!usb_handle->ClaimInterface(interface_id))
    return NULL;

  base::string16 serial;
  if (!usb_handle->GetSerial(&serial) || serial.empty())
    return NULL;

  return new AndroidUsbDevice(rsa_key, usb_handle, base::UTF16ToASCII(serial),
                              inbound_address, outbound_address, zero_mask,
                              interface_id);
}

uint32 Checksum(const std::string& data) {
  unsigned char* x = (unsigned char*)data.data();
  int count = data.length();
  uint32 sum = 0;
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

void ReleaseInterface(scoped_refptr<UsbDeviceHandle> usb_device,
                      int interface_id) {
  usb_device->ReleaseInterface(interface_id);
  usb_device->Close();
}

}  // namespace

AdbMessage::AdbMessage(uint32 command,
                       uint32 arg0,
                       uint32 arg1,
                       const std::string& body)
    : command(command),
      arg0(arg0),
      arg1(arg1),
      body(body) {
}

AdbMessage::~AdbMessage() {
}

static void RespondOnCallerThread(const AndroidUsbDevicesCallback& callback,
                                  AndroidUsbDevices* new_devices) {
  scoped_ptr<AndroidUsbDevices> devices(new_devices);

  // Add raw pointers to the newly claimed devices.
  for (AndroidUsbDevices::iterator it = devices->begin(); it != devices->end();
       ++it) {
    g_devices.Get().push_back(*it);
  }

  // Return all claimed devices.
  AndroidUsbDevices result(g_devices.Get().begin(), g_devices.Get().end());
  callback.Run(result);
}

static void RespondOnFileThread(
    const AndroidUsbDevicesCallback& callback,
    AndroidUsbDevices* devices,
    scoped_refptr<base::MessageLoopProxy> caller_message_loop_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  caller_message_loop_proxy->PostTask(
      FROM_HERE,
      base::Bind(&RespondOnCallerThread, callback, devices));
}

static void OpenAndroidDeviceOnFileThread(
    AndroidUsbDevices* devices,
    crypto::RSAPrivateKey* rsa_key,
    const base::Closure& barrier,
    scoped_refptr<UsbDevice> device,
    int interface_id,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (success) {
    scoped_refptr<UsbConfigDescriptor> config = device->ListInterfaces();
    scoped_refptr<UsbDeviceHandle> usb_handle = device->Open();
    if (usb_handle) {
      scoped_refptr<AndroidUsbDevice> android_device =
        ClaimInterface(rsa_key, usb_handle, config->GetInterface(interface_id),
                       interface_id);
      if (android_device.get())
        devices->push_back(android_device.get());
      else
        usb_handle->Close();
    }
  }
  barrier.Run();
}

static int CountOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  UsbService* service = UsbService::GetInstance();
  UsbDevices usb_devices;
  if (service != NULL)
    service->GetDevices(&usb_devices);
  int device_count = 0;
  for (UsbDevices::iterator it = usb_devices.begin(); it != usb_devices.end();
       ++it) {
    scoped_refptr<UsbConfigDescriptor> config = (*it)->ListInterfaces();
    if (!config)
      continue;

    for (size_t j = 0; j < config->GetNumInterfaces(); ++j) {
      if (IsAndroidInterface(config->GetInterface(j)))
        ++device_count;
    }
  }
  return device_count;
}

static void EnumerateOnFileThread(
    crypto::RSAPrivateKey* rsa_key,
    const AndroidUsbDevicesCallback& callback,
    scoped_refptr<base::MessageLoopProxy> caller_message_loop_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  UsbService* service = UsbService::GetInstance();
  UsbDevices usb_devices;
  if (service != NULL)
    service->GetDevices(&usb_devices);

  // Add new devices.
  AndroidUsbDevices* devices = new AndroidUsbDevices();
  base::Closure barrier = base::BarrierClosure(
      usb_devices.size(), base::Bind(&RespondOnFileThread,
                                     callback,
                                     devices,
                                     caller_message_loop_proxy));

  for (UsbDevices::iterator it = usb_devices.begin(); it != usb_devices.end();
       ++it) {
    scoped_refptr<UsbConfigDescriptor> config = (*it)->ListInterfaces();
    if (!config) {
      barrier.Run();
      continue;
    }

    bool has_android_interface = false;
    for (size_t j = 0; j < config->GetNumInterfaces(); ++j) {
      if (!IsAndroidInterface(config->GetInterface(j)))
        continue;

      // Request permission on Chrome OS.
#if defined(OS_CHROMEOS)
      (*it)->RequestUsbAccess(j, base::Bind(&OpenAndroidDeviceOnFileThread,
                                            devices, rsa_key, barrier, *it, j));
#else
      OpenAndroidDeviceOnFileThread(devices, rsa_key, barrier, *it, j, true);
#endif  // defined(OS_CHROMEOS)

      has_android_interface = true;
      break;
    }
    if (!has_android_interface)
      barrier.Run();
  }
}

// static
void AndroidUsbDevice::CountDevices(
    const base::Callback<void(int)>& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CountOnFileThread),
      callback);
}

// static
void AndroidUsbDevice::Enumerate(crypto::RSAPrivateKey* rsa_key,
                                 const AndroidUsbDevicesCallback& callback) {

  // Collect devices with closed handles.
  for (std::vector<AndroidUsbDevice*>::iterator it = g_devices.Get().begin();
       it != g_devices.Get().end(); ++it) {
    if ((*it)->usb_handle_) {
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&AndroidUsbDevice::TerminateIfReleased, *it,
                     (*it)->usb_handle_));
    }
  }

  // Then look for the new devices.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&EnumerateOnFileThread, rsa_key, callback,
                                     base::MessageLoopProxy::current()));
}

AndroidUsbDevice::AndroidUsbDevice(crypto::RSAPrivateKey* rsa_key,
                                   scoped_refptr<UsbDeviceHandle> usb_device,
                                   const std::string& serial,
                                   int inbound_address,
                                   int outbound_address,
                                   int zero_mask,
                                   int interface_id)
    : message_loop_(NULL),
      rsa_key_(rsa_key->Copy()),
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
  if (message_loop_)
    return;
  message_loop_ = base::MessageLoop::current();
  Queue(new AdbMessage(AdbMessage::kCommandCNXN, kVersion, kMaxPayload,
                       kHostConnectMessage));
  ReadHeader();
}

net::StreamSocket* AndroidUsbDevice::CreateSocket(const std::string& command) {
  if (!usb_handle_)
    return NULL;

  uint32 socket_id = ++last_socket_id_;
  sockets_[socket_id] = new AndroidUsbSocket(this, socket_id, command,
      base::Bind(&AndroidUsbDevice::SocketDeleted, this));
  return sockets_[socket_id];
}

void AndroidUsbDevice::Send(uint32 command,
                            uint32 arg0,
                            uint32 arg1,
                            const std::string& body) {
  scoped_refptr<AdbMessage> m = new AdbMessage(command, arg0, arg1, body);
  // Delay open request if not yet connected.
  if (!is_connected_) {
    pending_messages_.push_back(m);
    return;
  }
  Queue(m);
}

AndroidUsbDevice::~AndroidUsbDevice() {
  DCHECK(message_loop_ == base::MessageLoop::current());
  Terminate();
}

void AndroidUsbDevice::Queue(scoped_refptr<AdbMessage> message) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  // Queue header.
  std::vector<uint32> header;
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
  DCHECK(message_loop_ == base::MessageLoop::current());

  if (outgoing_queue_.empty() || !usb_handle_)
    return;

  BulkMessage message = outgoing_queue_.front();
  outgoing_queue_.pop();
  DumpMessage(true, message->data(), message->size());
  usb_handle_->BulkTransfer(
      usb_service::USB_DIRECTION_OUTBOUND, outbound_address_,
      message, message->size(), kUsbTimeout,
      base::Bind(&AndroidUsbDevice::OutgoingMessageSent,
                 weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::OutgoingMessageSent(UsbTransferStatus status,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           size_t result) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  if (status != usb_service::USB_TRANSFER_COMPLETED)
    return;
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::ProcessOutgoing, this));
}

void AndroidUsbDevice::ReadHeader() {
  DCHECK(message_loop_ == base::MessageLoop::current());

  if (!usb_handle_)
    return;
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kHeaderSize);
  usb_handle_->BulkTransfer(
      usb_service::USB_DIRECTION_INBOUND, inbound_address_,
      buffer, kHeaderSize, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseHeader,
                 weak_factory_.GetWeakPtr()));
}

void AndroidUsbDevice::ParseHeader(UsbTransferStatus status,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   size_t result) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  if (status == usb_service::USB_TRANSFER_TIMEOUT) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&AndroidUsbDevice::ReadHeader, this));
    return;
  }

  if (status != usb_service::USB_TRANSFER_COMPLETED || result != kHeaderSize) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer->data(), result);
  std::vector<uint32> header(6);
  memcpy(&header[0], buffer->data(), result);
  scoped_refptr<AdbMessage> message =
      new AdbMessage(header[0], header[1], header[2], "");
  uint32 data_length = header[3];
  uint32 data_check = header[4];
  uint32 magic = header[5];
  if ((message->command ^ 0xffffffff) != magic) {
    TransferError(usb_service::USB_TRANSFER_ERROR);
    return;
  }

  if (data_length == 0) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&AndroidUsbDevice::HandleIncoming, this,
                                       message));
    return;
  }

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::ReadBody, this,
                                     message, data_length, data_check));
}

void AndroidUsbDevice::ReadBody(scoped_refptr<AdbMessage> message,
                                uint32 data_length,
                                uint32 data_check) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  if (!usb_handle_)
    return;
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(data_length);
  usb_handle_->BulkTransfer(
      usb_service::USB_DIRECTION_INBOUND, inbound_address_,
      buffer, data_length, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseBody, weak_factory_.GetWeakPtr(),
                 message, data_length, data_check));
}

void AndroidUsbDevice::ParseBody(scoped_refptr<AdbMessage> message,
                                 uint32 data_length,
                                 uint32 data_check,
                                 UsbTransferStatus status,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 size_t result) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  if (status == usb_service::USB_TRANSFER_TIMEOUT) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&AndroidUsbDevice::ReadBody, this,
                            message, data_length, data_check));
    return;
  }

  if (status != usb_service::USB_TRANSFER_COMPLETED ||
      static_cast<uint32>(result) != data_length) {
    TransferError(status);
    return;
  }

  DumpMessage(false, buffer->data(), data_length);
  message->body = std::string(buffer->data(), result);
  if (Checksum(message->body) != data_check) {
    TransferError(usb_service::USB_TRANSFER_ERROR);
    return;
  }

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::HandleIncoming, this,
                                     message));
}

void AndroidUsbDevice::HandleIncoming(scoped_refptr<AdbMessage> message) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  switch (message->command) {
    case AdbMessage::kCommandAUTH:
      {
        DCHECK_EQ(message->arg0, static_cast<uint32>(AdbMessage::kAuthToken));
        if (signature_sent_) {
          Queue(new AdbMessage(AdbMessage::kCommandAUTH,
                               AdbMessage::kAuthRSAPublicKey, 0,
                               AndroidRSAPublicKey(rsa_key_.get())));
        } else {
          signature_sent_ = true;
          std::string signature = AndroidRSASign(rsa_key_.get(), message->body);
          if (!signature.empty()) {
            Queue(new AdbMessage(AdbMessage::kCommandAUTH,
                                 AdbMessage::kAuthSignature, 0,
                                 signature));
          } else {
            Queue(new AdbMessage(AdbMessage::kCommandAUTH,
                                 AdbMessage::kAuthRSAPublicKey, 0,
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
          Queue(*it);
        }
      }
      break;
    case AdbMessage::kCommandOKAY:
    case AdbMessage::kCommandWRTE:
    case AdbMessage::kCommandCLSE:
      {
        AndroidUsbSockets::iterator it = sockets_.find(message->arg1);
        if (it != sockets_.end())
          it->second->HandleIncoming(message);
      }
      break;
    default:
      break;
  }
  ReadHeader();
}

void AndroidUsbDevice::TransferError(UsbTransferStatus status) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::Terminate, this));
}

void AndroidUsbDevice::TerminateIfReleased(
    scoped_refptr<UsbDeviceHandle> usb_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (usb_handle->GetDevice())
    return;
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::Terminate, this));
}

void AndroidUsbDevice::Terminate() {
  DCHECK(message_loop_ == base::MessageLoop::current());

  std::vector<AndroidUsbDevice*>::iterator it =
      std::find(g_devices.Get().begin(), g_devices.Get().end(), this);
  if (it != g_devices.Get().end())
    g_devices.Get().erase(it);

  if (!usb_handle_)
    return;

  // Make sure we zero-out handle so that closing connections did not open
  // new connections.
  scoped_refptr<UsbDeviceHandle> usb_handle = usb_handle_;
  usb_handle_ = NULL;

  // Iterate over copy.
  AndroidUsbSockets sockets(sockets_);
  for (AndroidUsbSockets::iterator it = sockets.begin();
       it != sockets.end(); ++it) {
    it->second->Terminated();
  }
  DCHECK(sockets_.empty());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ReleaseInterface, usb_handle, interface_id_));
}

void AndroidUsbDevice::SocketDeleted(uint32 socket_id) {
  DCHECK(message_loop_ == base::MessageLoop::current());

  sockets_.erase(socket_id);
}
