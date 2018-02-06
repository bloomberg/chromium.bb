// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_media_analytics_client.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeMediaAnalyticsClient::FakeMediaAnalyticsClient()
    : process_running_(false), weak_ptr_factory_(this) {
  current_state_.set_status(mri::State::UNINITIALIZED);
}

FakeMediaAnalyticsClient::~FakeMediaAnalyticsClient() = default;

bool FakeMediaAnalyticsClient::FireMediaPerceptionEvent(
    const mri::MediaPerception& media_perception) {
  if (!process_running_)
    return false;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakeMediaAnalyticsClient::OnMediaPerception,
                            weak_ptr_factory_.GetWeakPtr(), media_perception));
  return true;
}

void FakeMediaAnalyticsClient::SetDiagnostics(
    const mri::Diagnostics& diagnostics) {
  diagnostics_ = diagnostics;
}

void FakeMediaAnalyticsClient::Init(dbus::Bus* bus) {}

void FakeMediaAnalyticsClient::SetMediaPerceptionSignalHandler(
    const MediaPerceptionSignalHandler& handler) {
  media_perception_signal_handler_ = handler;
}

void FakeMediaAnalyticsClient::ClearMediaPerceptionSignalHandler() {
  media_perception_signal_handler_.Reset();
}

void FakeMediaAnalyticsClient::GetState(
    DBusMethodCallback<mri::State> callback) {
  if (!process_running_) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeMediaAnalyticsClient::SetState(
    const mri::State& state,
    DBusMethodCallback<mri::State> callback) {
  if (!process_running_) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  DCHECK(state.has_status()) << "Trying to set state without status.";
  DCHECK(state.status() == mri::State::SUSPENDED ||
         state.status() == mri::State::RUNNING ||
         state.status() == mri::State::RESTARTING)
      << "Trying set state to something other than RUNNING, SUSPENDED or "
         "RESTARTING.";
  current_state_ = state;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeMediaAnalyticsClient::SetStateSuspended() {
  if (!process_running_) {
    return;
  }
  mri::State suspended;
  suspended.set_status(mri::State::SUSPENDED);
  current_state_ = suspended;
}

void FakeMediaAnalyticsClient::OnState(
    DBusMethodCallback<mri::State> callback) {
  std::move(callback).Run(current_state_);
}

void FakeMediaAnalyticsClient::GetDiagnostics(
    DBusMethodCallback<mri::Diagnostics> callback) {
  if (!process_running_) {
    LOG(ERROR) << "Fake media analytics process not running.";
    std::move(callback).Run(base::nullopt);
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnGetDiagnostics,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeMediaAnalyticsClient::OnGetDiagnostics(
    DBusMethodCallback<mri::Diagnostics> callback) {
  std::move(callback).Run(diagnostics_);
}

void FakeMediaAnalyticsClient::OnMediaPerception(
    const mri::MediaPerception& media_perception) {
  if (media_perception_signal_handler_.is_null())
    return;
  media_perception_signal_handler_.Run(media_perception);
}

}  // namespace chromeos
