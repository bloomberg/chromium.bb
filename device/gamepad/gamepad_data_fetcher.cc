// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_data_fetcher.h"

#include "base/time/time.h"

namespace device {
namespace {

void RunCallbackOnCallbackThread(
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    mojom::GamepadHapticsResult result) {
  std::move(callback).Run(result);
}
}  // namespace

GamepadDataFetcher::GamepadDataFetcher() : provider_(nullptr) {}

GamepadDataFetcher::~GamepadDataFetcher() = default;

void GamepadDataFetcher::InitializeProvider(GamepadPadStateProvider* provider) {
  DCHECK(provider);

  provider_ = provider;
  OnAddedToProvider();
}

void GamepadDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  RunVibrationCallback(std::move(callback), std::move(callback_runner),
                       mojom::GamepadHapticsResult::GamepadHapticsResultError);
}

void GamepadDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  RunVibrationCallback(std::move(callback), std::move(callback_runner),
                       mojom::GamepadHapticsResult::GamepadHapticsResultError);
}

// static
int64_t GamepadDataFetcher::TimeInMicroseconds(base::TimeTicks update_time) {
  return update_time.since_origin().InMicroseconds();
}

// static
int64_t GamepadDataFetcher::CurrentTimeInMicroseconds() {
  return TimeInMicroseconds(base::TimeTicks::Now());
}

// static
void GamepadDataFetcher::RunVibrationCallback(
    base::OnceCallback<void(mojom::GamepadHapticsResult)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner,
    mojom::GamepadHapticsResult result) {
  callback_runner->PostTask(FROM_HERE,
                            base::BindOnce(&RunCallbackOnCallbackThread,
                                           std::move(callback), result));
}

GamepadDataFetcherFactory::GamepadDataFetcherFactory() = default;

}  // namespace device
