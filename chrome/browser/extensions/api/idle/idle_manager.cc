// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/idle/idle_manager.h"

#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/extensions/api/idle/idle_api_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"

namespace keys = extensions::idle_api_constants;

namespace extensions {

namespace {

const int kDefaultIdleThreshold = 60;
const int kPollInterval = 1;

class DefaultEventDelegate : public IdleManager::EventDelegate {
 public:
  explicit DefaultEventDelegate(Profile* profile);
  virtual ~DefaultEventDelegate();

  virtual void OnStateChanged(const std::string& extension_id,
                              IdleState new_state) OVERRIDE;
  virtual void RegisterObserver(EventRouter::Observer* observer) OVERRIDE;
  virtual void UnregisterObserver(EventRouter::Observer* observer) OVERRIDE;

 private:
  Profile* profile_;
};

DefaultEventDelegate::DefaultEventDelegate(Profile* profile)
    : profile_(profile) {
}

DefaultEventDelegate::~DefaultEventDelegate() {
}

void DefaultEventDelegate::OnStateChanged(const std::string& extension_id,
                                          IdleState new_state) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(IdleManager::CreateIdleValue(new_state));
  scoped_ptr<Event> event(new Event(keys::kOnStateChanged, args.Pass()));
  event->restrict_to_profile = profile_;
  ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());
}

void DefaultEventDelegate::RegisterObserver(
    EventRouter::Observer* observer) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      observer, idle_api_constants::kOnStateChanged);
}

void DefaultEventDelegate::UnregisterObserver(EventRouter::Observer* observer) {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(observer);
}


class DefaultIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  DefaultIdleProvider();
  virtual ~DefaultIdleProvider();

  virtual void CalculateIdleState(int idle_threshold,
                                  IdleCallback notify) OVERRIDE;
  virtual void CalculateIdleTime(IdleTimeCallback notify) OVERRIDE;
  virtual bool CheckIdleStateIsLocked() OVERRIDE;
};

DefaultIdleProvider::DefaultIdleProvider() {
}

DefaultIdleProvider::~DefaultIdleProvider() {
}

void DefaultIdleProvider::CalculateIdleState(int idle_threshold,
                                             IdleCallback notify) {
  ::CalculateIdleState(idle_threshold, notify);
}

void DefaultIdleProvider::CalculateIdleTime(IdleTimeCallback notify) {
  ::CalculateIdleTime(notify);
}

bool DefaultIdleProvider::CheckIdleStateIsLocked() {
  return ::CheckIdleStateIsLocked();
}

IdleState IdleTimeToIdleState(bool locked, int idle_time, int idle_threshold) {
  IdleState state;

  if (locked) {
    state = IDLE_STATE_LOCKED;
  } else if (idle_time >= idle_threshold) {
    state = IDLE_STATE_IDLE;
  } else {
    state = IDLE_STATE_ACTIVE;
  }
  return state;
}

}  // namespace

IdleMonitor::IdleMonitor(IdleState initial_state)
    : last_state(initial_state),
      listeners(0),
      threshold(kDefaultIdleThreshold) {
}

IdleManager::IdleManager(Profile* profile)
    : profile_(profile),
      last_state_(IDLE_STATE_ACTIVE),
      weak_factory_(this),
      idle_time_provider_(new DefaultIdleProvider()),
      event_delegate_(new DefaultEventDelegate(profile)) {
}

IdleManager::~IdleManager() {
}

void IdleManager::Init() {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  event_delegate_->RegisterObserver(this);
}

void IdleManager::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_delegate_->UnregisterObserver(this);
}

void IdleManager::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const Extension* extension =
        content::Details<extensions::UnloadedExtensionInfo>(details)->extension;
    monitors_.erase(extension->id());
  } else {
    NOTREACHED();
  }
}

void IdleManager::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ++GetMonitor(details.extension_id)->listeners;
  StartPolling();
}

void IdleManager::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // During unload the monitor could already have been deleted. No need to do
  // anything in that case.
  MonitorMap::iterator it = monitors_.find(details.extension_id);
  if (it != monitors_.end()) {
    DCHECK_GT(it->second.listeners, 0);
    --it->second.listeners;
  }
}

void IdleManager::QueryState(int threshold, QueryStateCallback notify) {
  DCHECK(thread_checker_.CalledOnValidThread());
  idle_time_provider_->CalculateIdleState(threshold, notify);
}

void IdleManager::SetThreshold(const std::string& extension_id,
                               int threshold) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetMonitor(extension_id)->threshold = threshold;
}

// static
StringValue* IdleManager::CreateIdleValue(IdleState idle_state) {
  const char* description;

  if (idle_state == IDLE_STATE_ACTIVE) {
    description = keys::kStateActive;
  } else if (idle_state == IDLE_STATE_IDLE) {
    description = keys::kStateIdle;
  } else {
    description = keys::kStateLocked;
  }

  return new StringValue(description);
}

void IdleManager::SetEventDelegateForTest(
    scoped_ptr<EventDelegate> event_delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_delegate_ = event_delegate.Pass();
}

void IdleManager::SetIdleTimeProviderForTest(
    scoped_ptr<IdleTimeProvider> idle_time_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  idle_time_provider_ = idle_time_provider.Pass();
}

IdleMonitor* IdleManager::GetMonitor(const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MonitorMap::iterator it = monitors_.find(extension_id);

  if (it == monitors_.end()) {
    it = monitors_.insert(std::make_pair(extension_id,
                                         IdleMonitor(last_state_))).first;
  }
  return &it->second;
}

void IdleManager::StartPolling() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(kPollInterval),
                      this,
                      &IdleManager::UpdateIdleState);
  }
}

void IdleManager::StopPolling() {
  DCHECK(thread_checker_.CalledOnValidThread());
  poll_timer_.Stop();
}

void IdleManager::UpdateIdleState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  idle_time_provider_->CalculateIdleTime(
      base::Bind(
          &IdleManager::UpdateIdleStateCallback,
          weak_factory_.GetWeakPtr()));
}

void IdleManager::UpdateIdleStateCallback(int idle_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool locked = idle_time_provider_->CheckIdleStateIsLocked();
  int listener_count = 0;

  // Remember this state for initializing new event listeners.
  last_state_ = IdleTimeToIdleState(locked,
                                    idle_time,
                                    kDefaultIdleThreshold);

  for (MonitorMap::iterator it = monitors_.begin();
       it != monitors_.end(); ++it) {
    if (it->second.listeners < 1)
      continue;

    ++listener_count;

    IdleState new_state = IdleTimeToIdleState(locked,
                                              idle_time,
                                              it->second.threshold);

    if (new_state != it->second.last_state) {
      it->second.last_state = new_state;
      event_delegate_->OnStateChanged(it->first, new_state);
    }
  }

  if (listener_count == 0) {
    StopPolling();
  }
}

}  // namespace extensions
