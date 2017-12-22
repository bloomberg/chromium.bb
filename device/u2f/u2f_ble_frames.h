// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_BLE_FRAMES_H_
#define DEVICE_U2F_U2F_BLE_FRAMES_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "device/u2f/u2f_command_type.h"

namespace device {

class U2fBleFrameInitializationFragment;
class U2fBleFrameContinuationFragment;

// Encapsulates a frame, i.e., a single request to or response from a U2F
// authenticator, designed to be transported via BLE. The frame is further split
// into fragments (see U2fBleFrameFragment class).
//
// The specification of what constitues a frame can be found here:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-bt-protocol-v1.2-ps-20170411.html#h2_framing
//
// TODO(crbug/763303): Consider refactoring U2fMessage to support BLE frames.
class U2fBleFrame {
 public:
  // The values which can be carried in the |data| section of a KEEPALIVE
  // message sent from an authenticator.
  enum class KeepaliveCode : uint8_t {
    // The request is still being processed. The authenticator will be sending
    // this message every |kKeepAliveMillis| milliseconds until completion.
    PROCESSING = 0x01,
    // The authenticator is waiting for the Test of User Presence to complete.
    TUP_NEEDED = 0x02,
  };

  // The types of errors an authenticator can return to the client. Carried in
  // the |data| section of an ERROR command.
  enum class ErrorCode : uint8_t {
    INVALID_CMD = 0x01,  // The command in the request is unknown/invalid.
    INVALID_PAR = 0x02,  // The parameters of the command are invalid/missing.
    INVALID_LEN = 0x03,  // The length of the request is invalid.
    INVALID_SEQ = 0x04,  // The sequence number is invalid.
    REQ_TIMEOUT = 0x05,  // The request timed out.
    NA_1 = 0x06,         // Value reserved (HID).
    NA_2 = 0x0A,         // Value reserved (HID).
    NA_3 = 0x0B,         // Value reserved (HID).
    OTHER = 0x7F,        // Other, unspecified error.
  };

  U2fBleFrame();
  U2fBleFrame(U2fCommandType command, std::vector<uint8_t> data);

  U2fBleFrame(U2fBleFrame&&);
  U2fBleFrame& operator=(U2fBleFrame&&);

  ~U2fBleFrame();

  U2fCommandType command() const { return command_; }

  bool IsValid() const;
  KeepaliveCode GetKeepaliveCode() const;
  ErrorCode GetErrorCode() const;

  const std::vector<uint8_t>& data() const { return data_; }
  std::vector<uint8_t>& data() { return data_; }

  // Splits the frame into fragments suitable for sending over BLE. Returns the
  // first fragment via |initial_fragment|, and pushes the remaining ones back
  // to the |other_fragments| vector.
  //
  // The |max_fragment_size| parameter ought to be at least 3. The resulting
  // fragments' binary sizes will not exceed this value.
  std::pair<U2fBleFrameInitializationFragment,
            base::queue<U2fBleFrameContinuationFragment>>
  ToFragments(size_t max_fragment_size) const;

 private:
  U2fCommandType command_ = U2fCommandType::UNDEFINED;
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(U2fBleFrame);
};

// A single frame sent over BLE may be split over multiple writes and
// notifications because the technology was not designed for large messages.
// This class represents a single fragment. Not to be used directly.
//
// A frame is divided into an initialization fragment and zero, one or more
// continuation fragments. See the below section of the spec for the details:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-bt-protocol-v1.2-ps-20170411.html#h2_framing-fragmentation
//
// Note: This class and its subclasses don't own the |data|.
class U2fBleFrameFragment {
 public:
  base::span<const uint8_t> fragment() const { return fragment_; }
  virtual size_t Serialize(std::vector<uint8_t>* buffer) const = 0;

 protected:
  U2fBleFrameFragment();
  explicit U2fBleFrameFragment(base::span<const uint8_t> fragment);
  U2fBleFrameFragment(const U2fBleFrameFragment& frame);
  virtual ~U2fBleFrameFragment();

 private:
  base::span<const uint8_t> fragment_;
};

// An initialization fragment of a frame.
class U2fBleFrameInitializationFragment : public U2fBleFrameFragment {
 public:
  static bool Parse(base::span<const uint8_t> data,
                    U2fBleFrameInitializationFragment* fragment);

  U2fBleFrameInitializationFragment() = default;
  U2fBleFrameInitializationFragment(U2fCommandType command,
                                    uint16_t data_length,
                                    base::span<const uint8_t> fragment)
      : U2fBleFrameFragment(fragment),
        command_(command),
        data_length_(data_length) {}

  U2fCommandType command() const { return command_; }
  uint16_t data_length() const { return data_length_; }

  size_t Serialize(std::vector<uint8_t>* buffer) const override;

 private:
  U2fCommandType command_ = U2fCommandType::UNDEFINED;
  uint16_t data_length_ = 0;
};

// A continuation fragment of a frame.
class U2fBleFrameContinuationFragment : public U2fBleFrameFragment {
 public:
  static bool Parse(base::span<const uint8_t> data,
                    U2fBleFrameContinuationFragment* fragment);

  U2fBleFrameContinuationFragment() = default;
  U2fBleFrameContinuationFragment(base::span<const uint8_t> fragment,
                                  uint8_t sequence)
      : U2fBleFrameFragment(fragment), sequence_(sequence) {}

  uint8_t sequence() const { return sequence_; }

  size_t Serialize(std::vector<uint8_t>* buffer) const override;

 private:
  uint8_t sequence_ = 0;
};

// The helper used to construct a U2fBleFrame from a sequence of its fragments.
class U2fBleFrameAssembler {
 public:
  explicit U2fBleFrameAssembler(
      const U2fBleFrameInitializationFragment& fragment);
  ~U2fBleFrameAssembler();

  bool IsDone() const;

  bool AddFragment(const U2fBleFrameContinuationFragment& fragment);
  U2fBleFrame* GetFrame();

 private:
  uint16_t data_length_ = 0;
  uint8_t sequence_number_ = 0;
  U2fBleFrame frame_;

  DISALLOW_COPY_AND_ASSIGN(U2fBleFrameAssembler);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_BLE_FRAMES_H_
