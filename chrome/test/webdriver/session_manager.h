// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_SESSION_MANAGER_H_
#define CHROME_TEST_WEBDRIVER_SESSION_MANAGER_H_

#include <map>
#include <string>

#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "chrome/test/webdriver/session.h"

namespace webdriver {

// Session manager keeps track of all of the session that are currently
// running on the machine under test. With webdriver the user is allowed
// multiple instances of the browser on the same machine.  So 2 sessions
// open would mean you would have 2 instances of chrome running under
// their own profiles.
class SessionManager {
 public:
  // Returns the singleton instance.
  static SessionManager* GetInstance();

  std::string GetAddress();

  Session* Create();
  bool Delete(const std::string& id);
  bool Has(const std::string& id) const;

  Session* GetSession(const std::string& id) const;

  void set_port(const std::string& port) { port_ = port; }

 private:
  SessionManager();
  ~SessionManager();
  friend struct DefaultSingletonTraits<SessionManager>;

  std::map<std::string, Session*> map_;
  mutable base::Lock map_lock_;
  std::string port_;

  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_SESSION_MANAGER_H_
