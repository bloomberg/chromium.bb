/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_EVENT_EMITTER_H_

#include <stdint.h>

#include <map>
#include <set>

#include "nacl_io/error.h"

#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"


namespace nacl_io {

class EventEmitter;
class EventListener;

// A ref counted object (non POD derived from RefObject) for storing the
// state of a single signal request.  Requests are unique to any
// FD/EventListener pair.
struct EventInfo : public sdk_util::RefObject {
  // User provied data to be returned on EventListener::Wait
  uint64_t user_data;

  // Bitfield of POLL events currently signaled.
  uint32_t events;

  // Bitfield of POLL events of interest.
  uint32_t filter;

  // We do not use a ScopedRef to prevent circular references.
  EventEmitter* emitter;
  EventListener* listener;
  uint32_t id;
};

typedef sdk_util::ScopedRef<EventInfo> ScopedEventInfo;

// Provide comparison for std::map and std::set
bool operator<(const ScopedEventInfo& src_a, const ScopedEventInfo& src_b);

typedef std::map<int, ScopedEventInfo> EventInfoMap_t;
typedef std::set<ScopedEventInfo> EventInfoSet_t;

// EventEmitter
//
// The EventEmitter class provides notification of events to EventListeners
// by registering EventInfo objects and signaling the EventListener
// whenever thier state is changed.
//
// See "Kernel Events" in event_listener.h for additional information.
class EventEmitter : public sdk_util::RefObject {
 protected:
  // Called automatically prior to delete to inform the EventListeners that
  // this EventEmitter is abandoning an associated EventInfo.
  virtual void Destroy();

 private:
  // Register or unregister an EventInfo.  The lock of the EventListener
  // associated with this EventInfo must be held prior to calling these
  // functions.  These functions are private to ensure they are called by the
  // EventListener.
  void RegisterEventInfo(const ScopedEventInfo& info);
  void UnregisterEventInfo(const ScopedEventInfo& info);

 public:
  // Returns the current state of the emitter as POLL events bitfield.
  virtual uint32_t GetEventStatus() = 0;

  // Returns the type of the emitter (compatible with st_mode in stat)
  virtual int GetType() = 0;

 protected:
  // Called by the thread causing the Event.
  void RaiseEvent(uint32_t events);

  // Provided to allow one EventEmitter to register the same EventInfo with
  // a child EventEmitter so that they can both signal the EventListener.
  // Called after registering locally, but while lock is still held.
  virtual void ChainRegisterEventInfo(const ScopedEventInfo& event);

  // Called before unregistering locally, but while lock is still held.
  virtual void ChainUnregisterEventInfo(const ScopedEventInfo& event);

private:
  sdk_util::SimpleLock emitter_lock_;
  EventInfoSet_t events_;
  friend class EventListener;
};

typedef sdk_util::ScopedRef<EventEmitter> ScopedEventEmitter;

}  // namespace nacl_io


#endif  // LIBRARIES_NACL_IO_EVENT_EMITTER_H_