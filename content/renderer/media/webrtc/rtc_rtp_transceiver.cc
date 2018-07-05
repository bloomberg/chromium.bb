// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_transceiver.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"

namespace content {

RtpTransceiverState::RtpTransceiverState(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver,
    base::Optional<RtpSenderState> sender_state,
    base::Optional<RtpReceiverState> receiver_state,
    base::Optional<std::string> mid,
    bool stopped,
    webrtc::RtpTransceiverDirection direction,
    base::Optional<webrtc::RtpTransceiverDirection> current_direction)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      webrtc_transceiver_(std::move(webrtc_transceiver)),
      is_initialized_(false),
      sender_state_(std::move(sender_state)),
      receiver_state_(std::move(receiver_state)),
      mid_(std::move(mid)),
      stopped_(std::move(stopped)),
      direction_(std::move(direction)),
      current_direction_(std::move(current_direction)) {
  DCHECK(main_task_runner_);
  DCHECK(signaling_task_runner_);
  DCHECK(webrtc_transceiver_);
}

RtpTransceiverState::RtpTransceiverState(RtpTransceiverState&& other)
    : main_task_runner_(other.main_task_runner_),
      signaling_task_runner_(other.signaling_task_runner_),
      webrtc_transceiver_(std::move(other.webrtc_transceiver_)),
      is_initialized_(other.is_initialized_),
      sender_state_(std::move(other.sender_state_)),
      receiver_state_(std::move(other.receiver_state_)),
      mid_(std::move(other.mid_)),
      stopped_(std::move(other.stopped_)),
      direction_(std::move(other.direction_)),
      current_direction_(std::move(other.current_direction_)) {
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
}

RtpTransceiverState::~RtpTransceiverState() {
  // It's OK to not be on the main thread if this state has been moved, in which
  // case |main_task_runner_| is null.
  DCHECK(!main_task_runner_ || main_task_runner_->BelongsToCurrentThread());
}

RtpTransceiverState& RtpTransceiverState::operator=(
    RtpTransceiverState&& other) {
  DCHECK_EQ(main_task_runner_, other.main_task_runner_);
  DCHECK_EQ(signaling_task_runner_, other.signaling_task_runner_);
  // Need to be on main thread for sender/receiver state's destructor that can
  // be triggered by replacing .
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
  webrtc_transceiver_ = std::move(other.webrtc_transceiver_);
  is_initialized_ = other.is_initialized_;
  sender_state_ = std::move(other.sender_state_);
  receiver_state_ = std::move(other.receiver_state_);
  mid_ = std::move(other.mid_);
  stopped_ = std::move(other.stopped_);
  direction_ = std::move(other.direction_);
  current_direction_ = std::move(other.current_direction_);
  return *this;
}

bool RtpTransceiverState::is_initialized() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return is_initialized_;
}

void RtpTransceiverState::Initialize() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (sender_state_)
    sender_state_->Initialize();
  if (receiver_state_)
    receiver_state_->Initialize();
  is_initialized_ = true;
}

scoped_refptr<base::SingleThreadTaskRunner>
RtpTransceiverState::main_task_runner() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return main_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RtpTransceiverState::signaling_task_runner() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return signaling_task_runner_;
}

scoped_refptr<webrtc::RtpTransceiverInterface>
RtpTransceiverState::webrtc_transceiver() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return webrtc_transceiver_;
}

const base::Optional<RtpSenderState>& RtpTransceiverState::sender_state()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return sender_state_;
}

RtpSenderState RtpTransceiverState::MoveSenderState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::Optional<RtpSenderState> temp(base::nullopt);
  sender_state_.swap(temp);
  return *std::move(temp);
}

const base::Optional<RtpReceiverState>& RtpTransceiverState::receiver_state()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return receiver_state_;
}

RtpReceiverState RtpTransceiverState::MoveReceiverState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::Optional<RtpReceiverState> temp(base::nullopt);
  receiver_state_.swap(temp);
  return *std::move(temp);
}

base::Optional<std::string> RtpTransceiverState::mid() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return mid_;
}

bool RtpTransceiverState::stopped() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return stopped_;
}

webrtc::RtpTransceiverDirection RtpTransceiverState::direction() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return direction_;
}

base::Optional<webrtc::RtpTransceiverDirection>
RtpTransceiverState::current_direction() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return current_direction_;
}

}  // namespace content
