// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_ble_transaction.h"

#include <utility>

#include "device/fido/fido_constants.h"
#include "device/fido/u2f_ble_connection.h"

namespace device {

U2fBleTransaction::U2fBleTransaction(U2fBleConnection* connection,
                                     uint16_t control_point_length)
    : connection_(connection),
      control_point_length_(control_point_length),
      weak_factory_(this) {
  buffer_.reserve(control_point_length_);
}

U2fBleTransaction::~U2fBleTransaction() = default;

void U2fBleTransaction::WriteRequestFrame(U2fBleFrame request_frame,
                                          FrameCallback callback) {
  DCHECK(!request_frame_ && callback_.is_null());
  request_frame_ = std::move(request_frame);
  callback_ = std::move(callback);

  U2fBleFrameInitializationFragment request_init_fragment;
  std::tie(request_init_fragment, request_cont_fragments_) =
      request_frame_->ToFragments(control_point_length_);
  WriteRequestFragment(request_init_fragment);
}

void U2fBleTransaction::WriteRequestFragment(
    const U2fBleFrameFragment& fragment) {
  buffer_.clear();
  fragment.Serialize(&buffer_);
  // A weak pointer is required, since this call might time out. If that
  // happens, the current U2fBleTransaction could be destroyed.
  connection_->WriteControlPoint(
      buffer_, base::BindOnce(&U2fBleTransaction::OnRequestFragmentWritten,
                              weak_factory_.GetWeakPtr()));
  // WriteRequestFragment() expects an invocation of OnRequestFragmentWritten()
  // soon after.
  StartTimeout();
}

void U2fBleTransaction::OnRequestFragmentWritten(bool success) {
  StopTimeout();
  if (!success) {
    OnError();
    return;
  }

  if (request_cont_fragments_.empty()) {
    // The transaction wrote the full request frame. A response should follow
    // soon after.
    StartTimeout();
    return;
  }

  auto next_request_fragment = std::move(request_cont_fragments_.front());
  request_cont_fragments_.pop();
  WriteRequestFragment(next_request_fragment);
}

void U2fBleTransaction::OnResponseFragment(std::vector<uint8_t> data) {
  StopTimeout();
  if (!response_frame_assembler_) {
    U2fBleFrameInitializationFragment fragment;
    if (!U2fBleFrameInitializationFragment::Parse(data, &fragment)) {
      DLOG(ERROR) << "Malformed Frame Initialization Fragment";
      OnError();
      return;
    }

    response_frame_assembler_.emplace(fragment);
  } else {
    U2fBleFrameContinuationFragment fragment;
    if (!U2fBleFrameContinuationFragment::Parse(data, &fragment)) {
      DLOG(ERROR) << "Malformed Frame Continuation Fragment";
      OnError();
      return;
    }

    response_frame_assembler_->AddFragment(fragment);
  }

  if (!response_frame_assembler_->IsDone()) {
    // Expect the next reponse fragment to arrive soon.
    StartTimeout();
    return;
  }

  U2fBleFrame frame = std::move(*response_frame_assembler_->GetFrame());
  response_frame_assembler_.reset();
  ProcessResponseFrame(std::move(frame));
}

void U2fBleTransaction::ProcessResponseFrame(U2fBleFrame response_frame) {
  if (response_frame.command() == request_frame_->command()) {
    request_frame_.reset();
    std::move(callback_).Run(std::move(response_frame));
    return;
  }

  if (response_frame.command() == FidoBleDeviceCommand::kKeepAlive) {
    DVLOG(2) << "CMD_KEEPALIVE: "
             << static_cast<uint8_t>(response_frame.GetKeepaliveCode());
    // Expect another reponse frame soon.
    StartTimeout();
    return;
  }

  DCHECK_EQ(response_frame.command(), FidoBleDeviceCommand::kError);
  DLOG(ERROR) << "CMD_ERROR: "
              << static_cast<uint8_t>(response_frame.GetErrorCode());
  OnError();
}

void U2fBleTransaction::StartTimeout() {
  timer_.Start(FROM_HERE, kDeviceTimeout, this, &U2fBleTransaction::OnError);
}

void U2fBleTransaction::StopTimeout() {
  timer_.Stop();
}

void U2fBleTransaction::OnError() {
  request_frame_.reset();
  request_cont_fragments_ = {};
  response_frame_assembler_.reset();
  std::move(callback_).Run(base::nullopt);
}

}  // namespace device
