// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOG_ROUTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOG_ROUTER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerLogger;

// The router stands between PasswordManagerClient instances and log receivers.
// During the process of saving a password, the password manager code generates
// the log strings, and passes them to the router. The router distributes the
// logs to the receivers for displaying.
//
// TODO(vabr): The receivers are objects of type PasswordManagerLogger. That
// type should be renamed to LogReceiver instead.
class LogRouter {
 public:
  LogRouter();
  virtual ~LogRouter();

  // Passes logs to the router. Only call when there are receivers registered.
  void ProcessLog(const std::string& text);

  // All four (Unr|R)egister* methods below are safe to call from the
  // constructor of the registered object, because they do not call that object,
  // and the router only runs on a single thread.

  // The clients must register to be notified about whether there are some
  // receivers or not. RegisterClient adds |client| to the right observer list
  // and returns true iff there are some receivers registered.
  bool RegisterClient(PasswordManagerClient* client);
  // Remove |client| from the observers list.
  void UnregisterClient(PasswordManagerClient* client);

  // The receivers must register to get updates with new logs in the future.
  // RegisterReceiver adds |receiver| to the right observer list, and returns
  // the logs accumulated so far. (It returns by value, not const ref, to
  // provide a snapshot as opposed to a link to |accumulated_logs_|.)
  std::string RegisterReceiver(PasswordManagerLogger* receiver);
  // Remove |receiver| from the observers list.
  void UnregisterReceiver(PasswordManagerLogger* receiver);

 private:
  // Observer lists for clients and receivers. The |true| in the template
  // specialisation means that they will check that all observers were removed
  // on destruction.
  ObserverList<PasswordManagerClient, true> clients_;
  ObserverList<PasswordManagerLogger, true> receivers_;

  // Logs accumulated since the first receiver was registered.
  std::string accumulated_logs_;

  DISALLOW_COPY_AND_ASSIGN(LogRouter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOG_ROUTER_H_
