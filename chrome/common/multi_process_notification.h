// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MULTI_PROCESS_NOTIFICATION_H_
#define CHROME_COMMON_MULTI_PROCESS_NOTIFICATION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_ptr.h"

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
    virtual ~Delegate() { }
    virtual void OnNotificationReceived(const std::string& name,
                                        Domain domain) = 0;
  };

  Listener(const std::string& name, Domain domain, Delegate* delegate);

  // A destructor is required for scoped_ptr to compile.
  ~Listener();

  bool Start();

 private:
  scoped_ptr<ListenerImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(Listener);
};

// A delegate implementation that performs a task when a notification is
// received. Note that it does not check the notification, and will fire
// for any notification it receives. It will only fire the task on the
// first notification received.
class PerformTaskOnNotification : public Listener::Delegate {
 public:
  explicit PerformTaskOnNotification(Task* task);
  virtual ~PerformTaskOnNotification();

  virtual void OnNotificationReceived(const std::string& name,
                                      Domain domain) OVERRIDE;
  bool WasNotificationReceived();

 private:
  scoped_ptr<Task> task_;

  DISALLOW_COPY_AND_ASSIGN(PerformTaskOnNotification);
};

}  // namespace multi_process_notification

#endif  // CHROME_COMMON_MULTI_PROCESS_NOTIFICATION_H_
