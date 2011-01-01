// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DEPRECATED_EVENT_SYS_INL_H_
#define CHROME_COMMON_DEPRECATED_EVENT_SYS_INL_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/port.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "chrome/common/deprecated/event_sys.h"

// How to use Channels:

// 0. Assume Bob is the name of the class from which you want to broadcast
//    events.
// 1. Choose an EventType. This could be an Enum or something more complicated.
// 2. Create an EventTraits class for your EventType. It must have
//    two members:
//
//      typedef x EventType;
//      static bool IsChannelShutdownEvent(const EventType& event);
//
// 3. Add an EventChannel<MyEventTraits>* instance and event_channel() const;
//    accessor to Bob.
//    Delete the channel ordinarily in Bob's destructor, or whenever you want.
// 4. To broadcast events, call bob->event_channel()->NotifyListeners(event).
// 5. Only call NotifyListeners from a single thread at a time.

// How to use Listeners/Hookups:

// 0. Assume you want a class called Lisa to listen to events from Bob.
// 1. Create an event handler method in Lisa. Its single argument should be of
//    your event type.
// 2. Add a EventListenerHookup* hookup_ member to Lisa.
// 3. Initialize the hookup by calling:
//
//      hookup_ = NewEventListenerHookup(bob->event_channel(),
//                                       this,
//                                       &Lisa::HandleEvent);
//
// 4. Delete hookup_ in Lisa's destructor, or anytime sooner to stop receiving
//    events.

// An Event Channel is a source, or broadcaster of events. Listeners subscribe
// by calling the AddListener() method. The owner of the channel calls the
// NotifyListeners() method.
//
// Don't inherit from this class. Just make an event_channel member and an
// event_channel() accessor.

// No reason why CallbackWaiters has to be templatized.
class CallbackWaiters {
 public:
  CallbackWaiters() : waiter_count_(0),
                      callback_done_(false),
                      condvar_(&mutex_) {
  }
  ~CallbackWaiters() {
    DCHECK_EQ(0, waiter_count_);
  }
  void WaitForCallbackToComplete(base::Lock* listeners_mutex) {
    {
      base::AutoLock lock(mutex_);
      waiter_count_ += 1;
      listeners_mutex->Release();
      while (!callback_done_)
        condvar_.Wait();
      waiter_count_ -= 1;
      if (0 != waiter_count_)
        return;
    }
    delete this;
  }

  void Signal() {
    base::AutoLock lock(mutex_);
    callback_done_ = true;
    condvar_.Broadcast();
  }

 protected:
  int waiter_count_;
  bool callback_done_;
  base::Lock mutex_;
  base::ConditionVariable condvar_;
};

template <typename EventTraitsType, typename NotifyLock,
          typename ScopedNotifyLocker>
class EventChannel {
 public:
  typedef EventTraitsType EventTraits;
  typedef typename EventTraits::EventType EventType;
  typedef EventListener<EventType> Listener;

 protected:
  typedef std::map<Listener*, bool> Listeners;

 public:
  // The shutdown event gets send in the EventChannel's destructor.
  explicit EventChannel(const EventType& shutdown_event)
    : current_listener_callback_(NULL),
      current_listener_callback_message_loop_(NULL),
      callback_waiters_(NULL),
      shutdown_event_(shutdown_event) {
  }

  ~EventChannel() {
    // Tell all the listeners that the channel is being deleted.
    NotifyListeners(shutdown_event_);

    // Make sure all the listeners have been disconnected. Otherwise, they
    // will try to call RemoveListener() at a later date.
#if defined(DEBUG)
    base::AutoLock lock(listeners_mutex_);
    for (typename Listeners::iterator i = listeners_.begin();
         i != listeners_.end(); ++i) {
      DCHECK(i->second) << "Listener not disconnected";
    }
#endif
  }

  // Never call this twice for the same listener.
  //
  // Thread safe.
  void AddListener(Listener* listener) {
    base::AutoLock lock(listeners_mutex_);
    typename Listeners::iterator found = listeners_.find(listener);
    if (found == listeners_.end()) {
      listeners_.insert(std::make_pair(listener,
                                       false));  // Not dead yet.
    } else {
      DCHECK(found->second) << "Attempted to add the same listener twice.";
      found->second = false;  // Not dead yet.
    }
  }

  // If listener's callback is currently executing, this method waits until the
  // callback completes before returning.
  //
  // Thread safe.
  void RemoveListener(Listener* listener) {
    bool wait = false;
    listeners_mutex_.Acquire();
    typename Listeners::iterator found = listeners_.find(listener);
    if (found != listeners_.end()) {
      found->second = true;  // Mark as dead.
      wait = (found->first == current_listener_callback_ &&
          (MessageLoop::current() != current_listener_callback_message_loop_));
    }
    if (!wait) {
      listeners_mutex_.Release();
      return;
    }
    if (NULL == callback_waiters_)
      callback_waiters_ = new CallbackWaiters;
    callback_waiters_->WaitForCallbackToComplete(&listeners_mutex_);
  }

  // Blocks until all listeners have been notified.
  //
  // NOT thread safe.  Must only be called by one thread at a time.
  void NotifyListeners(const EventType& event) {
    ScopedNotifyLocker lock_notify(notify_lock_);
    listeners_mutex_.Acquire();
    DCHECK(NULL == current_listener_callback_);
    current_listener_callback_message_loop_ = MessageLoop::current();
    typename Listeners::iterator i = listeners_.begin();
    while (i != listeners_.end()) {
      if (i->second) {  // Clean out dead listeners
        listeners_.erase(i++);
        continue;
      }
      current_listener_callback_ = i->first;
      listeners_mutex_.Release();

      i->first->HandleEvent(event);

      listeners_mutex_.Acquire();
      current_listener_callback_ = NULL;
      if (NULL != callback_waiters_) {
        callback_waiters_->Signal();
        callback_waiters_ = NULL;
      }

      ++i;
    }
    listeners_mutex_.Release();
  }

  // A map iterator remains valid until the element it points to gets removed
  // from the map, so a map is perfect for our needs.
  //
  // Map value is a bool, true means the Listener is dead.
  Listeners listeners_;
  // NULL means no callback is currently being called.
  Listener* current_listener_callback_;
  // Only valid when current_listener is not NULL.
  // The thread on which the callback is executing.
  MessageLoop* current_listener_callback_message_loop_;
  // Win32 Event that is usually NULL. Only created when another thread calls
  // Remove while in callback. Owned and closed by the thread calling Remove().
  CallbackWaiters* callback_waiters_;

  base::Lock listeners_mutex_;  // Protects all members above.
  const EventType shutdown_event_;
  NotifyLock notify_lock_;

  DISALLOW_COPY_AND_ASSIGN(EventChannel);
};

// An EventListenerHookup hooks up a method in your class to an EventChannel.
// Deleting the hookup disconnects from the EventChannel.
//
// Contains complexity of inheriting from Listener class and managing lifetimes.
//
// Create using NewEventListenerHookup() to avoid explicit template arguments.
class EventListenerHookup {
 public:
  virtual ~EventListenerHookup() { }
};

template <typename EventChannel, typename EventTraits,
          class Derived>
class EventListenerHookupImpl : public EventListenerHookup,
public EventListener<typename EventTraits::EventType> {
 public:
  explicit EventListenerHookupImpl(EventChannel* channel)
    : channel_(channel), deleted_(NULL) {
    channel->AddListener(this);
    connected_ = true;
  }

  ~EventListenerHookupImpl() {
    if (NULL != deleted_)
      *deleted_ = true;
    if (connected_)
      channel_->RemoveListener(this);
  }

  typedef typename EventTraits::EventType EventType;
  virtual void HandleEvent(const EventType& event) {
    DCHECK(connected_);
    bool deleted = false;
    deleted_ = &deleted;
    static_cast<Derived*>(this)->Callback(event);
    if (deleted)  // The callback (legally) deleted this.
      return;  // The only safe thing to do.
    deleted_ = NULL;
    if (EventTraits::IsChannelShutdownEvent(event)) {
      channel_->RemoveListener(this);
      connected_ = false;
    }
  }

 protected:
  EventChannel* const channel_;
  bool connected_;
  bool* deleted_;  // Allows the handler to delete the hookup.
};

// SimpleHookup just passes the event to the callback message.
template <typename EventChannel, typename EventTraits,
          typename CallbackObject, typename CallbackMethod>
class SimpleHookup
    : public EventListenerHookupImpl<EventChannel, EventTraits,
                                     SimpleHookup<EventChannel,
                                                  EventTraits,
                                                  CallbackObject,
                                                  CallbackMethod> > {
 public:
  SimpleHookup(EventChannel* channel, CallbackObject* cbobject,
               CallbackMethod cbmethod)
    : EventListenerHookupImpl<EventChannel, EventTraits,
                              SimpleHookup>(channel), cbobject_(cbobject),
    cbmethod_(cbmethod) { }

  typedef typename EventTraits::EventType EventType;
  void Callback(const EventType& event) {
    (cbobject_->*cbmethod_)(event);
  }
  CallbackObject* const cbobject_;
  CallbackMethod const cbmethod_;
};

// ArgHookup also passes an additional arg to the callback method.
template <typename EventChannel, typename EventTraits,
          typename CallbackObject, typename CallbackMethod,
          typename CallbackArg0>
class ArgHookup
    : public EventListenerHookupImpl<EventChannel, EventTraits,
                                     ArgHookup<EventChannel, EventTraits,
                                               CallbackObject,
                                               CallbackMethod,
                                               CallbackArg0> > {
 public:
  ArgHookup(EventChannel* channel, CallbackObject* cbobject,
            CallbackMethod cbmethod, CallbackArg0 arg0)
    : EventListenerHookupImpl<EventChannel, EventTraits,
                              ArgHookup>(channel), cbobject_(cbobject),
      cbmethod_(cbmethod), arg0_(arg0) { }

  typedef typename EventTraits::EventType EventType;
  void Callback(const EventType& event) {
    (cbobject_->*cbmethod_)(arg0_, event);
  }
  CallbackObject* const cbobject_;
  CallbackMethod const cbmethod_;
  CallbackArg0 const arg0_;
};


template <typename EventChannel, typename CallbackObject,
          typename CallbackMethod>
EventListenerHookup* NewEventListenerHookup(EventChannel* channel,
                                            CallbackObject* cbobject,
                                            CallbackMethod cbmethod) {
  return new SimpleHookup<EventChannel,
    typename EventChannel::EventTraits,
    CallbackObject, CallbackMethod>(channel, cbobject, cbmethod);
}

template <typename EventChannel, typename CallbackObject,
          typename CallbackMethod, typename CallbackArg0>
EventListenerHookup* NewEventListenerHookup(EventChannel* channel,
                                            CallbackObject* cbobject,
                                            CallbackMethod cbmethod,
                                            CallbackArg0 arg0) {
  return new ArgHookup<EventChannel,
    typename EventChannel::EventTraits,
    CallbackObject, CallbackMethod, CallbackArg0>(channel, cbobject,
                                                  cbmethod, arg0);
}

#endif  // CHROME_COMMON_DEPRECATED_EVENT_SYS_INL_H_
