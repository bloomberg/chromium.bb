// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_CHANNEL_H_
#define CHROME_BROWSER_SYNC_UTIL_CHANNEL_H_
#pragma once

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A threadsafe container for a list of observers.  Observers are able to
//   remove themselves during iteration, and can be added on any thread.  This
//   allows observers to safely remove themselves during notifications.  It
//   also provides a handler when an observer is added that will remove the
//   observer on destruction.
//
//   It is expected that all observers are removed before destruction.
//   The channel owner should notify all observers to disconnect on shutdown if
//   needed to ensure this.
//
// TYPICAL USAGE:
//
//   class MyWidget {
//    public:
//     ...
//
//     class Observer : public ChannelEventHandler<FooEvent> {
//      public:
//       virtual void HandleChannelEvent(const FooEvent& w) = 0;
//     };
//
//     ChannelHookup<MyEvent>* AddObserver(Observer* obs) {
//       return channel_.AddObserver(obs);
//     }
//
//     void RemoveObserver(Observer* obs) {
//       channel_.RemoveObserver(obs);
//     }
//
//     void NotifyFoo(FooEvent& event) {
//       channel_.Notify(event);
//     }
//
//    private:
//     Channel<FooEvent> channel_;
//   };
//
//
///////////////////////////////////////////////////////////////////////////////

#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

namespace browser_sync {

template <typename EventType>
class Channel;

class EventHandler {
};

template <typename EventType>
class ChannelEventHandler : public EventHandler {
 public:
  virtual void HandleChannelEvent(const EventType& event) = 0;

 protected:
  virtual ~ChannelEventHandler() {}
};

// This class manages a connection to a channel.  When it is destroyed, it
// will remove the listener from the channel observer list.
template <typename EventType>
class ChannelHookup {
 public:
  ChannelHookup(Channel<EventType>* channel,
                browser_sync::ChannelEventHandler<EventType>* handler)
      : channel_(channel),
        handler_(handler) {}
  ~ChannelHookup() {
    channel_->RemoveObserver(handler_);
  }

 private:
  Channel<EventType>* channel_;
  browser_sync::ChannelEventHandler<EventType>* handler_;
};

template <typename EventType>
class Channel {
 public:
  typedef ObserverListBase<EventHandler> ChannelObserverList;

  Channel() : locking_thread_(0) {}

  ChannelHookup<EventType>* AddObserver(
      ChannelEventHandler<EventType>* observer) {
    base::AutoLock scoped_lock(event_handlers_mutex_);
    event_handlers_.AddObserver(observer);
    return new ChannelHookup<EventType>(this, observer);
  }

  void RemoveObserver(ChannelEventHandler<EventType>* observer) {
    // This can be called in response to a notification, so we may already have
    // locked this channel on this thread.
    bool need_lock = (locking_thread_ != base::PlatformThread::CurrentId());
    if (need_lock)
      event_handlers_mutex_.Acquire();

    event_handlers_mutex_.AssertAcquired();
    event_handlers_.RemoveObserver(observer);
    if (need_lock)
      event_handlers_mutex_.Release();
  }

  void Notify(const EventType& event) {
    base::AutoLock scoped_lock(event_handlers_mutex_);

    // This may result in an observer trying to remove itself, so keep track
    // of the thread we're locked on.
    locking_thread_ = base::PlatformThread::CurrentId();

    ChannelObserverList::Iterator it(event_handlers_);
    EventHandler* obs;
    while ((obs = it.GetNext()) != NULL) {
      static_cast<ChannelEventHandler<EventType>* >(obs)->
          HandleChannelEvent(event);
    }

    // Set back to an invalid thread id.
    locking_thread_ = 0;
  }

 private:
  base::Lock event_handlers_mutex_;
  base::PlatformThreadId locking_thread_;
  ObserverList<EventHandler> event_handlers_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_CHANNEL_H_
