// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_BLE_TRANSACTION_H_
#define DEVICE_U2F_U2F_BLE_TRANSACTION_H_

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "device/u2f/u2f_ble_frames.h"

namespace device {

class U2fBleConnection;

// This class encapsulates logic related to a single U2F BLE request and
// response. U2fBleTransaction is owned by U2fBleDevice, which is the only class
// that should make use of this class.
class U2fBleTransaction {
 public:
  using FrameCallback = base::OnceCallback<void(base::Optional<U2fBleFrame>)>;

  U2fBleTransaction(U2fBleConnection* connection,
                    uint16_t control_point_length);
  ~U2fBleTransaction();

  void WriteRequestFrame(U2fBleFrame request_frame, FrameCallback callback);
  void OnResponseFragment(std::vector<uint8_t> data);

 private:
  void WriteRequestFragment(const U2fBleFrameFragment& fragment);
  void OnRequestFragmentWritten(bool success);
  void ProcessResponseFrame(U2fBleFrame response_frame);

  void StartTimeout();
  void StopTimeout();
  void OnError();

  U2fBleConnection* connection_;
  uint16_t control_point_length_;

  base::Optional<U2fBleFrame> request_frame_;
  FrameCallback callback_;

  base::queue<U2fBleFrameContinuationFragment> request_cont_fragments_;
  base::Optional<U2fBleFrameAssembler> response_frame_assembler_;

  std::vector<uint8_t> buffer_;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<U2fBleTransaction> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_BLE_TRANSACTION_H_
