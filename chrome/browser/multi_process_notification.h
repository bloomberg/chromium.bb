// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTI_PROCESS_NOTIFICATION_H_
#define CHROME_BROWSER_MULTI_PROCESS_NOTIFICATION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"

class Task;
class MessageLoop;

// Platform abstraction for a notification that can be sent between processes.
// Notifications are strings. The string will be prefixed accordingly per
// platform (so on Mac OS X a "Happy" notification will become
// "org.chromium.Happy").
namespace multi_process_notification {

class ListenerImpl;

enum Domain {
  // Notifications intended to be received by processes running with the
  // same uid and same profile.
  ProfileDomain,
  // Notifications intended to be received by processes running with the
  // same uid.
  UserDomain,
  // Notifications intended to be received by processes running on the
  // same system.
  SystemDomain
};

// Posts a notification |name| to |domain|.
// Returns true if the notification was posted.
bool Post(const std::string& name, Domain domain);

// A notification listener. Will listen for a given notification and
// call the delegate. Note that the delegate is not owned by the listener.
class Listener {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // OnNotificationReceived is called on the thread that called Start() on the
    // Listener associated with this delegate.
    virtual void OnNotificationReceived(const std::string& name,
                                        Domain domain) = 0;

    // OnListenerStarted is called on the thread that called Start() on the
    // Listener associated with this delegate. If success is false, there
    // was an error starting the listener, and it is invalid.
    virtual void OnListenerStarted(
        const std::string& name, Domain domain, bool success);
  };

  class ListenerStartedTask : public Task {
   public:
    ListenerStartedTask(const std::string& name, Domain domain,
        Delegate* delegate, bool success);
    virtual ~ListenerStartedTask();
    virtual void Run();

  private:
    std::string name_;
    Domain domain_;
    Delegate* delegate_;
    bool success_;
    DISALLOW_COPY_AND_ASSIGN(ListenerStartedTask);
  };

  class NotificationReceivedTask : public Task {
   public:
    NotificationReceivedTask(
        const std::string& name, Domain domain, Delegate* delegate);
    virtual ~NotificationReceivedTask();
    virtual void Run();

  private:
    std::string name_;
    Domain domain_;
    Delegate* delegate_;
    DISALLOW_COPY_AND_ASSIGN(NotificationReceivedTask);
  };

  Listener(const std::string& name, Domain domain, Delegate* delegate);

  // A destructor is required for scoped_ptr to compile.
  ~Listener();

  // A listener is not considered valid until Start() returns true and its
  // delegate's OnListenerStarted method is called with |success| == true.
  // |io_loop_to_listen_on| is the message loop to register the listener on.
  // All callbacks will be made in the same thread that "Start" is called on,
  // not the thread that owns io_loop_to_listen_on.
  // |io_loop_to_listen_on| must be of type TYPE_IO.
  bool Start(MessageLoop* io_loop_to_listen_on);

  std::string name() const;
  Domain domain() const;

 private:
  scoped_ptr<ListenerImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(Listener);
};

}  // namespace multi_process_notification

#endif  // CHROME_BROWSER_MULTI_PROCESS_NOTIFICATION_H_
