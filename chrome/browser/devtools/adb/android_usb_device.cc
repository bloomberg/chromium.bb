// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/adb/android_usb_device.h"

#include "base/base64.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/adb/android_usb_socket.h"
#include "chrome/browser/usb/usb_interface.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/browser/usb/usb_service_factory.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace {

void Noop() {}
void BoolNoop(bool success) {}

const size_t kHeaderSize = 24;

const int kAdbClass = 0xff;
const int kAdbSubclass = 0x42;
const int kAdbProtocol = 0x1;

const int kUsbTimeout = 1000;

const uint32 kMaxPayload = 4096;
const uint32 kVersion = 0x01000000;

static const char kHostConnectMessage[] = "host::";
static const char kRSAPublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA6OSJ64q+ZLg7VV2ojEPh5TRbYjwbT"
    "TifSPeFIV45CHnbTWYiiIn41wrozpYizNsMWZUBjdah1N78WVhbyDrnr0bDgFp+gXjfVppa3I"
    "gjiohEcemK3omXi3GDMK8ERhriLUKfQS842SXtQ8I+KoZtpCkGM//0h7+P+Rhm0WwdipIRMhR"
    "8haNAeyDiiCvqJcvevv2T52vqKtS3aWz+GjaTJJLVWydEpz9WdvWeLfFVhe2ZnqwwZNa30Qoj"
    "fsnvjaMwK2MU7uYfRBPuvLyK5QESWBpArNDd6ULl8Y+NU6kwNOVDc87OASCVEM1gw2IMi2mo2"
    "WO5ywp0UWRiGZCkK+wOFQIDAQAB";

static bool CheckUsbInterface(
    const UsbInterface* interface,
    int* inbound_address,
    int* outbound_address,
    int* zero_mask) {
  if (interface->GetNumAltSettings() == 0)
    return false;

  scoped_refptr<const UsbInterfaceDescriptor> idesc =
      interface->GetAltSetting(0).get();

  if (idesc->GetInterfaceClass() != kAdbClass ||
      idesc->GetInterfaceSubclass() != kAdbSubclass ||
      idesc->GetInterfaceProtocol() != kAdbProtocol ||
      idesc->GetNumEndpoints() != 2) {
    return false;
  }

  for (size_t i = 0; i < idesc->GetNumEndpoints(); ++i) {
    scoped_refptr<const UsbEndpointDescriptor> edesc =
        idesc->GetEndpoint(i).get();
    if (edesc->GetTransferType() != USB_TRANSFER_BULK)
      continue;
    if (edesc->GetDirection() == USB_DIRECTION_INBOUND)
      *inbound_address = edesc->GetAddress();
    else
      *outbound_address = edesc->GetAddress();
    *zero_mask = edesc->GetMaximumPacketSize() - 1;
  }

  if (inbound_address == 0 || outbound_address == 0)
    return false;
  return true;
}

static uint32 Checksum(const std::string& data) {
  unsigned char* x = (unsigned char*)data.data();
  int count = data.length();
  uint32 sum = 0;
  while (count-- > 0)
    sum += *x++;
  return sum;
}

static void DumpMessage(bool outgoing, const char* data, size_t length) {
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

// static
void AndroidUsbDevice::Enumerate(
    Profile* profile,
    std::vector<scoped_refptr<AndroidUsbDevice> >* devices) {
  UsbService* service =
      UsbServiceFactory::GetInstance()->GetForProfile(profile);

  // Enumerate usb devices.
  std::vector<scoped_refptr<UsbDevice> > usb_devices;
  service->EnumerateDevices(&usb_devices);
  for (size_t i = 0; i < usb_devices.size(); ++i) {
    scoped_refptr<UsbDevice> usb_device = usb_devices[i];

    // Enumerate device interfaces.
    scoped_refptr<UsbConfigDescriptor> config = new UsbConfigDescriptor();
    usb_device->ListInterfaces(config.get(), base::Bind(&BoolNoop));
    for (size_t i = 0; i < config->GetNumInterfaces(); ++i) {
      scoped_refptr<const UsbInterface> interface = config->GetInterface(i);

      int inbound_address = 0;
      int outbound_address = 0;
      int zero_mask = 0;
      if (CheckUsbInterface(interface, &inbound_address, &outbound_address,
                            &zero_mask)) {
        devices->push_back(new AndroidUsbDevice(usb_device, inbound_address,
                                                outbound_address, zero_mask));
      }
    }
  }
}

AndroidUsbDevice::AndroidUsbDevice(scoped_refptr<UsbDevice> usb_device,
                                   int inbound_address,
                                   int outbound_address,
                                   int zero_mask)
    : message_loop_(NULL),
      usb_device_(usb_device),
      inbound_address_(inbound_address),
      outbound_address_(outbound_address),
      zero_mask_(zero_mask),
      is_connected_(false),
      last_socket_id_(256) {
  message_loop_ = base::MessageLoop::current();
  usb_device_->ClaimInterface(1, base::Bind(&AndroidUsbDevice::InterfaceClaimed,
                                            this));
}

net::StreamSocket* AndroidUsbDevice::CreateSocket(const std::string& command) {
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
  usb_device_->ReleaseInterface(1, base::Bind(&BoolNoop));
  usb_device_->Close(base::Bind(&Noop));
}

void AndroidUsbDevice::InterfaceClaimed(bool success) {
  if (!success)
    return;
  Queue(new AdbMessage(AdbMessage::kCommandCNXN, kVersion, kMaxPayload,
                       kHostConnectMessage));
  ReadHeader();
}

void AndroidUsbDevice::Queue(scoped_refptr<AdbMessage> message) {
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
  scoped_refptr<net::IOBuffer> header_buffer = new net::IOBuffer(kHeaderSize);
  memcpy(header_buffer.get()->data(), &header[0], kHeaderSize);
  outgoing_queue_.push(std::make_pair(header_buffer, kHeaderSize));

  // Queue body.
  if (!message->body.empty()) {
    scoped_refptr<net::IOBuffer> body_buffer = new net::IOBuffer(body_length);
    memcpy(body_buffer->data(), message->body.data(), message->body.length());
    if (append_zero)
      body_buffer->data()[body_length - 1] = 0;
    outgoing_queue_.push(std::make_pair(body_buffer, body_length));
  }
  ProcessOutgoing();
}

void AndroidUsbDevice::ProcessOutgoing() {
  if (outgoing_queue_.empty())
    return;

  BulkMessage message = outgoing_queue_.front();
  outgoing_queue_.pop();
  DumpMessage(true, message.first->data(), message.second);
  usb_device_->BulkTransfer(USB_DIRECTION_OUTBOUND, outbound_address_,
      message.first, message.second, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::OutgoingMessageSent, this));
}

void AndroidUsbDevice::OutgoingMessageSent(UsbTransferStatus status,
                                           scoped_refptr<net::IOBuffer> buffer,
                                           size_t result) {
  if (status != USB_TRANSFER_COMPLETED)
    return;
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::ProcessOutgoing,
                                     this));
}

void AndroidUsbDevice::ReadHeader() {
  if (HasOneRef())
    return;  // Stop polling.
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kHeaderSize);
  usb_device_->BulkTransfer(USB_DIRECTION_INBOUND, inbound_address_,
      buffer, kHeaderSize, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseHeader, this));
}

void AndroidUsbDevice::ParseHeader(UsbTransferStatus status,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   size_t result) {
  if (status == USB_TRANSFER_TIMEOUT) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&AndroidUsbDevice::ReadHeader, this));
    return;
  }

  if (status != USB_TRANSFER_COMPLETED) {
    LOG(ERROR) << "Unexpeced transfer status: " << status;
    return;
  }

  if (result != kHeaderSize) {
    LOG(ERROR) << "Unexpeced header size: " << result;
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
  if ((message->command ^ 0xffffffff) != magic)
    return;

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
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(data_length);
  usb_device_->BulkTransfer(USB_DIRECTION_INBOUND, inbound_address_,
      buffer, data_length, kUsbTimeout,
      base::Bind(&AndroidUsbDevice::ParseBody, this, message, data_length,
                 data_check));
}

void AndroidUsbDevice::ParseBody(scoped_refptr<AdbMessage> message,
                                 uint32 data_length,
                                 uint32 data_check,
                                 UsbTransferStatus status,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 size_t result) {
  if (status == USB_TRANSFER_TIMEOUT) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&AndroidUsbDevice::ReadBody, this,
                            message, data_length, data_check));
    return;
  }

  if (status != USB_TRANSFER_COMPLETED) {
    LOG(ERROR) << "Unexpeced transfer status: " << status;
    return;
  }

  if (static_cast<uint32>(result) != data_length) {
    LOG(ERROR) << "Unexpeced body length: " << result << ", expecteced" <<
        data_length;
    return;
  }

  DumpMessage(false, buffer->data(), data_length);
  message->body = std::string(buffer->data(), result);
  if (Checksum(message->body) != data_check) {
    LOG(ERROR) << "Wrong body checksum";
    return;
  }

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AndroidUsbDevice::HandleIncoming, this,
                                     message));
}

void AndroidUsbDevice::HandleIncoming(scoped_refptr<AdbMessage> message) {
  switch (message->command) {
    case AdbMessage::kCommandAUTH:
      {
        DCHECK_EQ(message->arg0, static_cast<uint32>(AdbMessage::kAuthToken));
        Queue(new AdbMessage(AdbMessage::kCommandAUTH,
                             AdbMessage::kAuthRSAPublicKey, 0,
                             kRSAPublicKey));
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
        // Route these to sockets
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

void AndroidUsbDevice::SocketDeleted(uint32 socket_id) {
  sockets_.erase(socket_id);
}
