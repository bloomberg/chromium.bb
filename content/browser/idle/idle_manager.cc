// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/idle/idle_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/idle/idle_monitor.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "ui/base/idle/idle.h"

namespace content {

namespace {

const int kPollInterval = 1;

// Default provider implementation. Everything is delegated to
// ui::CalculateIdleState, ui::CalculateIdleTime, and
// ui::CheckIdleStateIsLocked.
class DefaultIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  DefaultIdleProvider() = default;
  ~DefaultIdleProvider() override = default;

  ui::IdleState CalculateIdleState(int idle_threshold) override {
    return ui::CalculateIdleState(idle_threshold);
  }

  int CalculateIdleTime() override { return ui::CalculateIdleTime(); }

  bool CheckIdleStateIsLocked() override {
    return ui::CheckIdleStateIsLocked();
  }
};

blink::mojom::IdleStatePtr IdleTimeToIdleState(bool locked,
                                               int idle_time,
                                               int idle_threshold) {
  blink::mojom::UserIdleState user;
  if (idle_time >= idle_threshold)
    user = blink::mojom::UserIdleState::IDLE;
  else
    user = blink::mojom::UserIdleState::ACTIVE;

  blink::mojom::ScreenIdleState screen;
  if (locked)
    screen = blink::mojom::ScreenIdleState::LOCKED;
  else
    screen = blink::mojom::ScreenIdleState::UNLOCKED;

  return blink::mojom::IdleState::New(user, screen);
}

}  // namespace

IdleManager::IdleManager()
    : idle_time_provider_(new DefaultIdleProvider()), weak_factory_(this) {}

IdleManager::~IdleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!monitors_.empty()) {
    IdleMonitor* monitor = monitors_.head()->value();
    monitor->RemoveFromList();
    delete monitor;
  }
}

void IdleManager::CreateService(blink::mojom::IdleManagerRequest request,
                                const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(this, std::move(request));
}

void IdleManager::AddMonitor(uint32_t threshold,
                             blink::mojom::IdleMonitorPtr monitor_ptr,
                             AddMonitorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (threshold == 0) {
    mojo::ReportBadMessage("Invalid threshold");
    return;
  }

  auto monitor = std::make_unique<IdleMonitor>(
      std::move(monitor_ptr), CheckIdleState(threshold), threshold);

  // This unretained reference is safe because IdleManager owns all IdleMonitor
  // instances.
  monitor->SetErrorHandler(
      base::BindOnce(&IdleManager::RemoveMonitor, base::Unretained(this)));

  monitors_.Append(monitor.release());

  StartPolling();

  std::move(callback).Run(CheckIdleState(threshold));
}

void IdleManager::RemoveMonitor(IdleMonitor* monitor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  monitor->RemoveFromList();
  delete monitor;

  if (monitors_.empty()) {
    StopPolling();
  }
}

void IdleManager::SetIdleTimeProviderForTest(
    std::unique_ptr<IdleTimeProvider> idle_time_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_time_provider_ = std::move(idle_time_provider);
}

bool IdleManager::IsPollingForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return poll_timer_.IsRunning();
}

void IdleManager::StartPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kPollInterval),
                      base::BindRepeating(&IdleManager::UpdateIdleState,
                                          base::Unretained(this)));
  }
}

void IdleManager::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  poll_timer_.Stop();
}

blink::mojom::IdleStatePtr IdleManager::CheckIdleState(int threshold) {
  int idle_time = idle_time_provider_->CalculateIdleTime();
  bool locked = idle_time_provider_->CheckIdleStateIsLocked();

  return IdleTimeToIdleState(locked, idle_time, threshold);
}

void IdleManager::UpdateIdleState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* node = monitors_.head(); node != monitors_.end();
       node = node->next()) {
    IdleMonitor* monitor = node->value();
    monitor->SetLastState(CheckIdleState(monitor->threshold()));
  }
}

}  // namespace content
