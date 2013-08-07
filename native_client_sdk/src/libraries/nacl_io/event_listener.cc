/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>

#include "nacl_io/error.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/osstat.h"
#include "nacl_io/osunistd.h"

#include "sdk_util/auto_lock.h"


namespace nacl_io {

EventListener::EventListener() {
  pthread_cond_init(&signal_cond_, NULL);
}

EventListener::~EventListener() {
  pthread_cond_destroy(&signal_cond_);
}

// Before we can destroy ourselves, we must first unregister all the
// EventInfo objects from the various EventListeners
void EventListener::Destroy() {
  EventInfoMap_t::iterator it;

  // We do not take the lock since this is the only reference to this object.
  for (it = event_info_map_.begin(); it != event_info_map_.end(); it++) {
    if (it->second->emitter) {
      it->second->emitter->UnregisterEventInfo(it->second);
    }
  }

  EventEmitter::Destroy();
}

uint32_t EventListener::GetEventStatus() {
  // Always writable, but we can only assume it to be readable if there
  // is an event waiting.
  return signaled_.empty() ? POLLOUT : POLLIN | POLLOUT;
}

int EventListener::GetType() {
  // For lack of a better type, report socket to signify it can be in an
  // used to signal.
  return S_IFSOCK;
}

// Called by EventEmitter, wakes up any blocking threads to verify if the wait
// conditions have been met.
void EventListener::Signal(const ScopedEventInfo& info) {
  AUTO_LOCK(signal_lock_);
  if (waiting_) {
    signaled_.insert(info);
    pthread_cond_broadcast(&signal_cond_);
  }
}

static void AbsoluteFromDeltaMS(struct timespec* timeout, int ms_timeout) {
  if (ms_timeout >= 0) {
    uint64_t usec = usec_since_epoch();
    usec += ((int64_t) ms_timeout * 1000);

    timeout->tv_nsec = (usec % 1000000) * 1000;
    timeout->tv_sec = (usec / 1000000);
  } else {
    timeout->tv_sec = 0;
    timeout->tv_nsec = 0;
  }
}

Error EventListener::Wait(EventData* events,
                          int max,
                          int ms_timeout,
                          int* out_count) {
  *out_count = 0;

  if (max <= 0)
    return EINVAL;

  if (NULL == events)
    return EFAULT;

  {
    AUTO_LOCK(info_lock_);

    // Go through the "live" event infos and see if they are in a signaled state
    EventInfoMap_t::iterator it = event_info_map_.begin();
    while ((it != event_info_map_.end()) && (*out_count < max)) {
      ScopedEventInfo& info = it->second;
      uint32_t event_bits = info->emitter->GetEventStatus() & info->filter;

      if (event_bits) {
        events[*out_count].events = event_bits;
        events[*out_count].user_data = info->user_data;
        (*out_count)++;
      }

      it++;
    }
  } // End of info_lock scope.

  // We are done if we have a signal or no timeout specified.
  if ((*out_count > 0) || (0 == ms_timeout))
    return 0;

  // Compute the absolute time we can wait until.
  struct timespec timeout;
  AbsoluteFromDeltaMS(&timeout, ms_timeout);

  // Keep looking if until we receive something.
  while (0 == *out_count) {
    // We are now officially waiting.
    AUTO_LOCK(signal_lock_);
    waiting_++;

    // If we don't have any signals yet, wait for any Emitter to Signal.
    while (signaled_.empty()) {
      int return_code;
      if (ms_timeout >= 0) {
        return_code = pthread_cond_timedwait(&signal_cond_,
                                             signal_lock_.mutex(),
                                             &timeout);
      } else {
        return_code = pthread_cond_wait(&signal_cond_, signal_lock_.mutex());
      }

      Error error(return_code);

      // If there is no error, then we may have been signaled.
      if (0 == error)
        break;

      // For any error case:
      if (ETIMEDOUT == error) {
        // A "TIMEOUT" is not an error.
        error = 0;
      } else {
        // Otherwise this has gone bad, so return EBADF.
        error = EBADF;
      }

      waiting_--;
      return error;
    }

    // Copy signals over as long as we have room
    while (!signaled_.empty() && (*out_count < max)) {
      EventInfoSet_t::iterator it = signaled_.begin();

      events[*out_count].events = (*it)->events;
      events[*out_count].user_data = (*it)->user_data;
      (*out_count)++;

      signaled_.erase(it);
    }

    // If we are the last thread waiting, clear out the signalled set
    if (1 == waiting_)
      signaled_.clear();

    // We are done waiting.
    waiting_--;
  }

  return 0;
}

Error EventListener::Track(int id,
                          const ScopedEventEmitter& emitter,
                          uint32_t filter,
                          uint64_t user_data) {
  AUTO_LOCK(info_lock_);
  EventInfoMap_t::iterator it = event_info_map_.find(id);

  // If it's not a streaming type, then it can not be added.
  if ((emitter->GetType() & (S_IFIFO | S_IFSOCK)) == 0)
    return EPERM;

  if (it != event_info_map_.end())
    return EEXIST;

  if (emitter.get() == this)
    return EINVAL;

  ScopedEventInfo info(new EventInfo);
  info->emitter = emitter.get();
  info->listener = this;
  info->id = id;
  info->filter = filter;
  info->user_data = user_data;
  info->events = 0;

  emitter->RegisterEventInfo(info);
  event_info_map_[id] = info;
  return 0;
}

Error EventListener::Update(int id, uint32_t filter, uint64_t user_data) {
  AUTO_LOCK(info_lock_);
  EventInfoMap_t::iterator it = event_info_map_.find(id);
  if (it == event_info_map_.end())
    return ENOENT;

  ScopedEventInfo& info = it->second;
  info->filter = filter;
  info->user_data = user_data;
  return 0;
}

Error EventListener::Free(int id) {
  AUTO_LOCK(info_lock_);
  EventInfoMap_t::iterator it = event_info_map_.find(id);
  if (event_info_map_.end() == it)
    return ENOENT;

  it->second->emitter->UnregisterEventInfo(it->second);
  event_info_map_.erase(it);
  return 0;
}

void EventListener::AbandonedEventInfo(const ScopedEventInfo& event) {
  {
    AUTO_LOCK(info_lock_);

    event->emitter = NULL;
    event_info_map_.erase(event->id);
  }

  // EventInfos abandoned by the destroyed emitter must still be kept in
  // signaled_ set for POLLHUP.
  event->events = POLLHUP;
  Signal(event);
}

}  // namespace nacl_io
