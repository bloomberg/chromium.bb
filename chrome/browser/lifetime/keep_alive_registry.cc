// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/keep_alive_registry.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/keep_alive_state_observer.h"
#include "chrome/browser/lifetime/keep_alive_types.h"

////////////////////////////////////////////////////////////////////////////////
// Public methods

// static
KeepAliveRegistry* KeepAliveRegistry::GetInstance() {
  return base::Singleton<KeepAliveRegistry>::get();
}

bool KeepAliveRegistry::IsKeepingAlive() const {
  return registered_count_ > 0;
}

bool KeepAliveRegistry::IsRestartAllowed() const {
  return registered_count_ == restart_allowed_count_;
}

bool KeepAliveRegistry::IsOriginRegistered(KeepAliveOrigin origin) const {
  return registered_keep_alives_.find(origin) != registered_keep_alives_.end();
}

void KeepAliveRegistry::AddObserver(KeepAliveStateObserver* observer) {
  observers_.AddObserver(observer);
}

void KeepAliveRegistry::RemoveObserver(KeepAliveStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

KeepAliveRegistry::KeepAliveRegistry()
    : registered_count_(0), restart_allowed_count_(0) {}

KeepAliveRegistry::~KeepAliveRegistry() {
  DLOG_IF(ERROR, registered_count_ > 0 || registered_keep_alives_.size() > 0)
      << "KeepAliveRegistry not empty at destruction time. State:" << *this;
}

void KeepAliveRegistry::Register(KeepAliveOrigin origin,
                                 KeepAliveRestartOption restart) {
  DCHECK(!g_browser_process->IsShuttingDown());

  bool old_keeping_alive = IsKeepingAlive();
  bool old_restart_allowed = IsRestartAllowed();

  ++registered_keep_alives_[origin];
  ++registered_count_;

  if (restart == KeepAliveRestartOption::ENABLED)
    ++restart_allowed_count_;

  bool new_keeping_alive = IsKeepingAlive();
  bool new_restart_allowed = IsRestartAllowed();

  if (new_keeping_alive != old_keeping_alive)
    OnKeepAliveStateChanged(new_keeping_alive);

  if (new_restart_allowed != old_restart_allowed)
    OnRestartAllowedChanged(new_restart_allowed);

  DVLOG(1) << "New state of the KeepAliveRegistry: " << *this;
}

void KeepAliveRegistry::Unregister(KeepAliveOrigin origin,
                                   KeepAliveRestartOption restart) {
  bool old_keeping_alive = IsKeepingAlive();
  bool old_restart_allowed = IsRestartAllowed();

  --registered_count_;
  DCHECK_GE(registered_count_, 0);

  int new_count = --registered_keep_alives_[origin];
  DCHECK_GE(registered_keep_alives_[origin], 0);
  if (new_count == 0)
    registered_keep_alives_.erase(origin);

  if (restart == KeepAliveRestartOption::ENABLED)
    --restart_allowed_count_;

  bool new_keeping_alive = IsKeepingAlive();
  bool new_restart_allowed = IsRestartAllowed();

  if (new_restart_allowed != old_restart_allowed)
    OnRestartAllowedChanged(new_restart_allowed);

  if (new_keeping_alive != old_keeping_alive)
    OnKeepAliveStateChanged(new_keeping_alive);

  DVLOG(1) << "New state of the KeepAliveRegistry:" << *this;
}

void KeepAliveRegistry::OnKeepAliveStateChanged(bool new_keeping_alive) {
  DVLOG(1) << "Notifying KeepAliveStateObservers: KeepingAlive changed to: "
           << new_keeping_alive;
  FOR_EACH_OBSERVER(KeepAliveStateObserver, observers_,
                    OnKeepAliveStateChanged(new_keeping_alive));
}

void KeepAliveRegistry::OnRestartAllowedChanged(bool new_restart_allowed) {
  DVLOG(1) << "Notifying KeepAliveStateObservers: Restart changed to: "
           << new_restart_allowed;
  FOR_EACH_OBSERVER(KeepAliveStateObserver, observers_,
                    OnKeepAliveRestartStateChanged(new_restart_allowed));
}

std::ostream& operator<<(std::ostream& out, const KeepAliveRegistry& registry) {
  out << "{registered_count_=" << registry.registered_count_
      << ", restart_allowed_count_=" << registry.restart_allowed_count_
      << ", KeepAlives=[";
  for (auto counts_per_origin_it : registry.registered_keep_alives_) {
    if (counts_per_origin_it != *registry.registered_keep_alives_.begin())
      out << ", ";
    out << counts_per_origin_it.first << " (" << counts_per_origin_it.second
        << ")";
  }
  out << "]}";
  return out;
}
