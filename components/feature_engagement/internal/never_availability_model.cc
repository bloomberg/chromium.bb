// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_availability_model.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace feature_engagement {

NeverAvailabilityModel::NeverAvailabilityModel() : ready_(false) {}

NeverAvailabilityModel::~NeverAvailabilityModel() = default;

void NeverAvailabilityModel::Initialize(OnInitializedCallback callback,
                                        uint32_t current_day) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&NeverAvailabilityModel::ForwardedOnInitializedCallback,
                     base::Unretained(this), std::move(callback)));
}

bool NeverAvailabilityModel::IsReady() const {
  return ready_;
}

base::Optional<uint32_t> NeverAvailabilityModel::GetAvailability(
    const base::Feature& feature) const {
  return base::nullopt;
}

void NeverAvailabilityModel::ForwardedOnInitializedCallback(
    OnInitializedCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
  ready_ = true;
}

}  // namespace feature_engagement
