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

  std::string GetIPAddress();
  bool SetIPAddress(const std::string& port);

  Session* Create();
  bool Delete(const std::string& id);
  bool Has(const std::string& id) const;

  Session* GetSession(const std::string& id) const;

 private:
  SessionManager();
  ~SessionManager();
  friend struct DefaultSingletonTraits<SessionManager>;
  std::string GenerateSessionID();
  std::string IPLookup(const std::string& nic);

  std::map<std::string, Session*> map_;
  mutable base::Lock map_lock_;
  base::Lock session_generation_;
  // Record the address and port for the HTTP 303 See other redirect.
  // We save the IP and Port of the machine chromedriver is running on since
  // a HTTP 303, see other,  resdirect is sent after a successful creation of
  // a session, ie: http://172.22.41.105:8080/session/DFSSE453CV588
  std::string addr_;
  std::string port_;
  int count_;

  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_SESSION_MANAGER_H_
