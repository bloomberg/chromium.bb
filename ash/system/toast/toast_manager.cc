// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

namespace {

// Minimum duration for a toast to be visible (in millisecond).
uint64_t kMinimumDurationMs = 200;

}  // anonymous namespace

ToastManager::ToastManager() : weak_ptr_factory_(this) {}

ToastManager::~ToastManager() {}

void ToastManager::Show(const std::string& text, uint64_t duration_ms) {
  queue_.emplace(std::make_pair(text, duration_ms));

  if (queue_.size() == 1 && overlay_ == nullptr)
    ShowLatest();
}

void ToastManager::OnClosed() {
  overlay_.reset();

  // Show the next toast if available.
  if (queue_.size() != 0)
    ShowLatest();
}

void ToastManager::ShowLatest() {
  DCHECK(!overlay_);

  auto data = queue_.front();
  uint64_t duration_ms = std::max(data.second, kMinimumDurationMs);

  toast_id_++;

  overlay_.reset(new ToastOverlay(this, data.first /* text */));
  overlay_->Show(true);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ToastManager::OnDurationPassed,
                            weak_ptr_factory_.GetWeakPtr(), toast_id_),
      base::TimeDelta::FromMilliseconds(duration_ms));

  queue_.pop();
}

void ToastManager::OnDurationPassed(int toast_id) {
  if (overlay_ && toast_id_ == toast_id)
    overlay_->Show(false);
}

}  // namespace ash
