/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_IO_EVENT_LISTENER_H_
#define LIBRARIES_NACL_IO_EVENT_LISTENER_H_

#include <pthread.h>

#include <map>
#include <set>
#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/event_emitter.h"

#include "sdk_util/scoped_ref.h"

// Kernel Events
//
// Certain file objects such as pipes or sockets can become signaled when
// read or write buffers become available, or when the connection is torn
// down.  EventListener provides a mechanism for a thread to wait on
// specific events from these objects which are derived from EventEmitters.
//
// EventEmitter and EventListener together provide support for an "epoll"
// like interface.  See:
//     http://man7.org/linux/man-pages/man7/epoll.7.html
//
// Such that we map the arguments at behavior of
//     epoll_wait maps to Wait, and
//     epoll_ctl maps to Track, Update, Free.
//
//  Behavior of EventListeners
//    FDs are automatically removed when closed.
//    KE_SHUTDOWN can not be masked.
//    KE_SHUTDOWN is only seen if the hangup happens after Wait starts.
//    Dup'd FDs get their own event info which must also get signaled.
//    Adding a non streaming FD will fail.
//    EventEmitters can also be waited on.
//    It is illegal for an a EventListener to add itself.
//
//  Locking:
//    EventListener::{Track/Update/Free}
//      AUTO_LOCK(EventListener::info_lock_)
//       EventEmitter::RegisterEventInfo
//         AUTO_LOCK(EventEmitter::emitter_lock_)
//
//    EventEmitter::Destroy
//      EventListener::AbandonedEventInfo
//        AUTO_LOCK(EventListener::info_lock_)
//
//    EventListener::RaiseEvent
//      AUTO_LOCK(EventEmitter::emitter_lock_)
//        EventListener::Signal
//          AUTO_LOCK(EventListener::signal_lock_)
//
//    EventListener::Wait
//      AUTO_LOCK(EventListener::info_lock_)
//        ...
//      AUTO_LOCK(EventListener::signal_lock_)
//        ...

namespace nacl_io {

struct EventData {
  // Bit Mask of signaled POLL events.
  uint32_t events;
  uint64_t user_data;
};


// EventListener
//
// The EventListener class provides an object to wait on for specific events
// from EventEmitter objects.  The EventListener becomes signalled for
// read when events are waiting, making it is also an Emitter.
class EventListener : public EventEmitter {
 public:
   EventListener();
   ~EventListener();

 protected:
  // Called prior to free to unregister all EventInfos from the EventEmitters.
  void Destroy();

 public:
   // Declared in EventEmitter
   virtual uint32_t GetEventStatus();
   virtual int GetType();

   // Called by EventEmitter to signal the Listener that a new event is
   // available.
   void Signal(const ScopedEventInfo& info);

   // Wait for one or more previously Tracked events to take place
   // or until ms_timeout expires, and fills |events| up to |max| limit.
   // The number of events recored is returned in |count|.
   Error Wait(EventData* events, int max, int ms_timeout, int* out_count);

   // Tracks a new set of POLL events for a given unique |id|.  The
   // |user_data| will be returned in the Wait when an event of type |filter|
   // is received with that |id|.
   Error Track(int id,
               const ScopedEventEmitter& emitter,
               uint32_t filter,
               uint64_t user_data);

   // Updates the tracking of events for |id|, replacing the |user_data|
   // that's returned, as well as which events will signal.
   Error Update(int id, uint32_t filter, uint64_t user_data);

   // Unregisters the existing |id|.
   Error Free(int id);

   // Notification by EventEmitter that it is abandoning the event.  Do not
   // access the emitter after this.
   void AbandonedEventInfo(const ScopedEventInfo& event);

 private:
  // Protects the data in the EventInfo map.
  sdk_util::SimpleLock info_lock_;

  // Map from ID to live a event info.
  EventInfoMap_t event_info_map_;

  // Protects waiting_, signaled_ and used with the signal_cond_.
  sdk_util::SimpleLock signal_lock_;
  pthread_cond_t signal_cond_;

  // The number of threads currently waiting on this Listener.
  uint32_t waiting_;

  // Set of event infos signaled during a wait.
  EventInfoSet_t signaled_;
};

typedef sdk_util::ScopedRef<EventListener> ScopedEventListener;

}  // namespace nacl_io

#endif  /* LIBRARIES_NACL_IO_EVENT_LISTENER_H_ */
