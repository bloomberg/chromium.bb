// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager.h"

#include "base/callback_helpers.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "ui/base/idle/idle.h"

namespace content {

namespace {

const int kDefaultIdleThreshold = 60;
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

blink::mojom::IdleState IdleTimeToIdleState(bool locked,
                                            int idle_time,
                                            int idle_threshold) {
  if (locked)
    return blink::mojom::IdleState::LOCKED;
  else if (idle_time >= idle_threshold)
    return blink::mojom::IdleState::IDLE;
  else
    return blink::mojom::IdleState::ACTIVE;
}

}  // namespace

class IdleManager::Monitor : public base::LinkNode<Monitor> {
 public:
  Monitor(blink::mojom::IdleMonitorPtr monitor,
          blink::mojom::IdleState last_state,
          uint32_t threshold)
      : client_(std::move(monitor)),
        last_state_(last_state),
        threshold_(threshold),
        weak_factory_(this) {}
  ~Monitor() = default;

  blink::mojom::IdleMonitorPtr& client() { return client_; }

  blink::mojom::IdleState last_state() const { return last_state_; }
  void set_last_state(blink::mojom::IdleState state) { last_state_ = state; }

  uint32_t threshold() const { return threshold_; }
  void set_threshold(uint32_t threshold) { threshold_ = threshold; }

  base::WeakPtr<Monitor> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  blink::mojom::IdleMonitorPtr client_;
  blink::mojom::IdleState last_state_;
  uint32_t threshold_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<Monitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Monitor);
};

IdleManager::IdleManager()
    : idle_time_provider_(new DefaultIdleProvider()), weak_factory_(this) {}

IdleManager::~IdleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!monitors_.empty()) {
    Monitor* monitor = monitors_.head()->value();
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

  auto monitor =
      std::make_unique<Monitor>(std::move(monitor_ptr), last_state_, threshold);
  monitor->client().set_connection_error_handler(
      base::BindOnce(&IdleManager::RemoveMonitor, base::Unretained(this),
                     base::Unretained(monitor.get())));
  monitors_.Append(monitor.release());

  StartPolling();
  std::move(callback).Run(last_state_);
}

void IdleManager::RemoveMonitor(Monitor* monitor) {
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

void IdleManager::UpdateIdleState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int idle_time = idle_time_provider_->CalculateIdleTime();

  bool locked = idle_time_provider_->CheckIdleStateIsLocked();
  // Remember this state for initializing new event listeners.
  last_state_ = IdleTimeToIdleState(locked, idle_time, kDefaultIdleThreshold);

  for (auto* node = monitors_.head(); node != monitors_.end();
       node = node->next()) {
    Monitor* monitor = node->value();

    auto new_state =
        IdleTimeToIdleState(locked, idle_time, monitor->threshold());
    if (monitor->last_state() != new_state) {
      monitor->set_last_state(new_state);
      monitor->client()->Update(new_state);
    }
  }
}

}  // namespace content
