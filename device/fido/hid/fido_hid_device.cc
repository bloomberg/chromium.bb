// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_device.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/hid/fido_hid_message.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {

namespace {
// U2F devices only provide a single report so specify a report ID of 0 here.
static constexpr uint8_t kReportId = 0x00;
}  // namespace

FidoHidDevice::FidoHidDevice(device::mojom::HidDeviceInfoPtr device_info,
                             device::mojom::HidManager* hid_manager)
    : FidoDevice(),
      output_report_size_(device_info->max_output_report_size),
      hid_manager_(hid_manager),
      device_info_(std::move(device_info)),
      weak_factory_(this) {
  DCHECK_GE(std::numeric_limits<decltype(output_report_size_)>::max(),
            device_info_->max_output_report_size);
  // These limits on the report size are enforced in fido_hid_discovery.cc.
  DCHECK_LT(kHidInitPacketHeaderSize, output_report_size_);
  DCHECK_GE(kHidMaxPacketSize, output_report_size_);
}

FidoHidDevice::~FidoHidDevice() = default;

void FidoHidDevice::DeviceTransact(std::vector<uint8_t> command,
                                   DeviceCallback callback) {
  pending_transactions_.emplace_back(std::move(command), std::move(callback));
  Transition();
}

void FidoHidDevice::Cancel() {
  // If device has not been connected or is already in error state, do nothing.
  if (state_ != State::kBusy && state_ != State::kReady)
    return;

  // TODO(agl): this is problematic:
  //   1) sends a cancel message, whether the transaction to be canceled is the
  //      active one or not.
  //   2) there's no way to indicate in the API which transaction should be
  //      canceled.
  //   3) the cancel is sent immediately, but that may corrupt a message that
  //      is in the process of being transmitted.
  state_ = State::kReady;
  timeout_callback_.Cancel();

  WriteMessage(
      FidoHidMessage::Create(channel_id_,
                             supported_protocol() == ProtocolVersion::kCtap
                                 ? FidoHidDeviceCommand::kCancel
                                 : FidoHidDeviceCommand::kInit,
                             output_report_size_, std::vector<uint8_t>()),
      false /* response_expected */, base::DoNothing());
}

// TODO(agl): maybe Transition should take the next step to move to?
void FidoHidDevice::Transition() {
  switch (state_) {
    case State::kInit:
      state_ = State::kBusy;
      ArmTimeout();
      Connect(base::BindOnce(&FidoHidDevice::OnConnect,
                             weak_factory_.GetWeakPtr()));
      break;
    case State::kConnected:
      state_ = State::kBusy;
      ArmTimeout();
      AllocateChannel();
      break;
    case State::kReady: {
      state_ = State::kBusy;
      DCHECK(!pending_transactions_.empty());
      ArmTimeout();

      // Write message to the device.
      const auto command_type = supported_protocol() == ProtocolVersion::kCtap
                                    ? FidoHidDeviceCommand::kCbor
                                    : FidoHidDeviceCommand::kMsg;
      WriteMessage(FidoHidMessage::Create(
                       channel_id_, command_type, output_report_size_,
                       std::move(pending_transactions_.front().command)),
                   true,
                   base::BindOnce(&FidoHidDevice::MessageReceived,
                                  weak_factory_.GetWeakPtr()));
      break;
    }
    case State::kBusy:
      break;
    case State::kDeviceError:
    case State::kMsgError:
      base::WeakPtr<FidoHidDevice> self = weak_factory_.GetWeakPtr();
      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_transactions_.empty()) {
        // Respond to any pending requests.
        DeviceCallback pending_cb =
            std::move(pending_transactions_.front().callback);
        pending_transactions_.pop_front();
        std::move(pending_cb).Run(base::nullopt);
      }
      break;
  }
}

FidoHidDevice::PendingTransaction::PendingTransaction(
    std::vector<uint8_t> in_command,
    DeviceCallback in_callback)
    : command(std::move(in_command)), callback(std::move(in_callback)) {}

FidoHidDevice::PendingTransaction::~PendingTransaction() = default;

void FidoHidDevice::Connect(ConnectCallback callback) {
  DCHECK(hid_manager_);
  hid_manager_->Connect(device_info_->guid, std::move(callback));
}

void FidoHidDevice::OnConnect(device::mojom::HidConnectionPtr connection) {
  if (state_ == State::kDeviceError)
    return;
  timeout_callback_.Cancel();

  if (connection) {
    connection_ = std::move(connection);
    state_ = State::kConnected;
  } else {
    state_ = State::kDeviceError;
  }
  Transition();
}

void FidoHidDevice::AllocateChannel() {
  // Send random nonce to device to verify received message.
  std::vector<uint8_t> nonce(8);
  crypto::RandBytes(nonce.data(), nonce.size());
  WriteMessage(FidoHidMessage::Create(channel_id_, FidoHidDeviceCommand::kInit,
                                      output_report_size_, nonce),
               true,
               base::BindOnce(&FidoHidDevice::OnAllocateChannel,
                              weak_factory_.GetWeakPtr(), nonce));
}

void FidoHidDevice::OnAllocateChannel(std::vector<uint8_t> nonce,
                                      base::Optional<FidoHidMessage> message) {
  if (state_ == State::kDeviceError)
    return;

  // TODO(agl): is this correct? it's entirely possible to see a response to a
  // command from another client on the machine. Going to kDeviceError seems
  // questionable.
  if (!message || message->cmd() != FidoHidDeviceCommand::kInit) {
    timeout_callback_.Cancel();
    state_ = State::kDeviceError;
    Transition();
    return;
  }

  // Channel allocation response is defined as:
  // 0: 8 byte nonce
  // 8: 4 byte channel id
  // 12: Protocol version id
  // 13: Major device version
  // 14: Minor device version
  // 15: Build device version
  // 16: Capabilities
  std::vector<uint8_t> payload = message->GetMessagePayload();
  if (payload.size() != 17) {
    timeout_callback_.Cancel();
    state_ = State::kDeviceError;
    Transition();
    return;
  }

  auto received_nonce = base::make_span(payload).first(8);
  // Received a broadcast message for a different client. Disregard and continue
  // reading.
  if (!std::equal(nonce.begin(), nonce.end(), received_nonce.begin(),
                  received_nonce.end())) {
    ReadMessage(base::BindOnce(&FidoHidDevice::OnAllocateChannel,
                               weak_factory_.GetWeakPtr(), std::move(nonce)));

    return;
  }

  timeout_callback_.Cancel();
  size_t index = 8;
  channel_id_ = payload[index++] << 24;
  channel_id_ |= payload[index++] << 16;
  channel_id_ |= payload[index++] << 8;
  channel_id_ |= payload[index++];
  capabilities_ = payload[16];
  state_ = State::kReady;
  Transition();
}

void FidoHidDevice::WriteMessage(base::Optional<FidoHidMessage> message,
                                 bool response_expected,
                                 HidMessageCallback callback) {
  if (!connection_ || !message || message->NumPackets() == 0) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  auto packet = message->PopNextPacket();
  DCHECK_LE(packet.size(), output_report_size_);
  packet.resize(output_report_size_, 0);
  connection_->Write(
      kReportId, packet,
      base::BindOnce(&FidoHidDevice::PacketWritten, weak_factory_.GetWeakPtr(),
                     std::move(message), response_expected,
                     std::move(callback)));
}

void FidoHidDevice::PacketWritten(base::Optional<FidoHidMessage> message,
                                  bool response_expected,
                                  HidMessageCallback callback,
                                  bool success) {
  if (success && message->NumPackets() > 0) {
    WriteMessage(std::move(message), response_expected, std::move(callback));
  } else if (success && response_expected) {
    ReadMessage(std::move(callback));
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

void FidoHidDevice::ReadMessage(HidMessageCallback callback) {
  if (!connection_) {
    state_ = State::kDeviceError;
    std::move(callback).Run(base::nullopt);
    return;
  }

  connection_->Read(base::BindOnce(
      &FidoHidDevice::OnRead, weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FidoHidDevice::OnRead(HidMessageCallback callback,
                           bool success,
                           uint8_t report_id,
                           const base::Optional<std::vector<uint8_t>>& buf) {
  if (!success) {
    state_ = State::kDeviceError;
    std::move(callback).Run(base::nullopt);
    return;
  }

  DCHECK(buf);
  auto read_message = FidoHidMessage::CreateFromSerializedData(*buf);
  if (!read_message) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Received a message from a different channel, so try again.
  if (channel_id_ != read_message->channel_id()) {
    connection_->Read(base::BindOnce(&FidoHidDevice::OnRead,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(callback)));
    return;
  }

  if (read_message->MessageComplete()) {
    std::move(callback).Run(std::move(read_message));
    return;
  }

  // Continue reading additional packets.
  connection_->Read(base::BindOnce(
      &FidoHidDevice::OnReadContinuation, weak_factory_.GetWeakPtr(),
      std::move(read_message), std::move(callback)));
}

void FidoHidDevice::OnReadContinuation(
    base::Optional<FidoHidMessage> message,
    HidMessageCallback callback,
    bool success,
    uint8_t report_id,
    const base::Optional<std::vector<uint8_t>>& buf) {
  if (!success) {
    state_ = State::kDeviceError;
    std::move(callback).Run(base::nullopt);
    return;
  }

  DCHECK(buf);
  message->AddContinuationPacket(*buf);
  if (message->MessageComplete()) {
    std::move(callback).Run(std::move(message));
    return;
  }
  connection_->Read(base::BindOnce(&FidoHidDevice::OnReadContinuation,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(message), std::move(callback)));
}

void FidoHidDevice::MessageReceived(base::Optional<FidoHidMessage> message) {
  if (state_ == State::kDeviceError)
    return;

  timeout_callback_.Cancel();
  if (!message) {
    state_ = State::kDeviceError;
    Transition();
    return;
  }

  const auto cmd = message->cmd();
  // If received HID packet is a keep-alive message then reset the timeout and
  // read again.
  if (supported_protocol() == ProtocolVersion::kCtap &&
      cmd == FidoHidDeviceCommand::kKeepAlive) {
    ArmTimeout();
    ReadMessage(base::BindOnce(&FidoHidDevice::MessageReceived,
                               weak_factory_.GetWeakPtr()));
    return;
  }

  auto response = message->GetMessagePayload();
  if (cmd != FidoHidDeviceCommand::kMsg && cmd != FidoHidDeviceCommand::kCbor) {
    // TODO(agl): inline |ProcessHidError|, or maybe have it call |Transition|.
    ProcessHidError(cmd, response);
    Transition();
    return;
  }

  state_ = State::kReady;
  DCHECK(!pending_transactions_.empty());
  auto callback = std::move(pending_transactions_.front().callback);
  pending_transactions_.pop_front();

  base::WeakPtr<FidoHidDevice> self = weak_factory_.GetWeakPtr();
  std::move(callback).Run(std::move(response));

  // Executing |callback| may have freed |this|. Check |self| first.
  if (self && !pending_transactions_.empty()) {
    Transition();
  }
}

void FidoHidDevice::TryWink(WinkCallback callback) {
  // Only try to wink if device claims support.
  if (!(capabilities_ & kWinkCapability) || state_ != State::kReady) {
    std::move(callback).Run();
    return;
  }

  WriteMessage(
      FidoHidMessage::Create(channel_id_, FidoHidDeviceCommand::kWink,
                             output_report_size_, std::vector<uint8_t>()),
      true,
      base::BindOnce(&FidoHidDevice::OnWink, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void FidoHidDevice::OnWink(WinkCallback callback,
                           base::Optional<FidoHidMessage> response) {
  std::move(callback).Run();
}

void FidoHidDevice::ArmTimeout() {
  DCHECK(timeout_callback_.IsCancelled());
  timeout_callback_.Reset(
      base::BindOnce(&FidoHidDevice::OnTimeout, weak_factory_.GetWeakPtr()));
  // Setup timeout task for 3 seconds.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_callback_.callback(), kDeviceTimeout);
}

void FidoHidDevice::OnTimeout() {
  state_ = State::kDeviceError;
  Transition();
}

void FidoHidDevice::ProcessHidError(FidoHidDeviceCommand cmd,
                                    base::span<const uint8_t> payload) {
  if (cmd != FidoHidDeviceCommand::kError || payload.size() != 1) {
    FIDO_LOG(ERROR) << "Unknown HID message received: " << static_cast<int>(cmd)
                    << " " << base::HexEncode(payload.data(), payload.size());
    state_ = State::kDeviceError;
    return;
  }

  switch (static_cast<HidErrorConstant>(payload[0])) {
    case HidErrorConstant::kInvalidCommand:
    case HidErrorConstant::kInvalidParameter:
    case HidErrorConstant::kInvalidLength:
      state_ = State::kMsgError;
      break;
    default:
      FIDO_LOG(ERROR) << "HID error received: " << static_cast<int>(payload[0]);
      state_ = State::kDeviceError;
  }
}

std::string FidoHidDevice::GetId() const {
  return GetIdForDevice(*device_info_);
}

FidoTransportProtocol FidoHidDevice::DeviceTransport() const {
  return FidoTransportProtocol::kUsbHumanInterfaceDevice;
}

// VidPidToString returns the device's vendor and product IDs as formatted by
// the lsusb utility.
static std::string VidPidToString(const mojom::HidDeviceInfoPtr& device_info) {
  static_assert(sizeof(device_info->vendor_id) == 2,
                "vendor_id must be uint16_t");
  static_assert(sizeof(device_info->product_id) == 2,
                "product_id must be uint16_t");
  uint16_t vendor_id = ((device_info->vendor_id & 0xff) << 8) |
                       ((device_info->vendor_id & 0xff00) >> 8);
  uint16_t product_id = ((device_info->product_id & 0xff) << 8) |
                        ((device_info->product_id & 0xff00) >> 8);
  return base::ToLowerASCII(base::HexEncode(&vendor_id, 2) + ":" +
                            base::HexEncode(&product_id, 2));
}

void FidoHidDevice::DiscoverSupportedProtocolAndDeviceInfo(
    base::OnceClosure done) {
  // The following devices cannot handle GetInfo messages.
  static const base::flat_set<std::string> kForceU2fCompatibilitySet({
      "10c4:8acf",  // U2F Zero
      "20a0:4287",  // Nitrokey FIDO U2F
  });

  if (base::ContainsKey(kForceU2fCompatibilitySet,
                        VidPidToString(device_info_))) {
    supported_protocol_ = ProtocolVersion::kU2f;
    DCHECK(SupportedProtocolIsInitialized());
    std::move(done).Run();
    return;
  }
  FidoDevice::DiscoverSupportedProtocolAndDeviceInfo(std::move(done));
}

// static
std::string FidoHidDevice::GetIdForDevice(
    const device::mojom::HidDeviceInfo& device_info) {
  return "hid:" + device_info.guid;
}

base::WeakPtr<FidoDevice> FidoHidDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
