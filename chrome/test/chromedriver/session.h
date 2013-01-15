// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"

class Chrome;

struct Session {
  explicit Session(const std::string& id);
  Session(const std::string& id, scoped_ptr<Chrome> chrome);
  ~Session();

  const std::string id;
  scoped_ptr<Chrome> chrome;
  std::string frame;
  int implicit_wait;
  int page_load_timeout;
  int script_timeout;
};

class SessionAccessor : public base::RefCountedThreadSafe<SessionAccessor> {
 public:
  virtual Session* Access(scoped_ptr<base::AutoLock>* lock) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionAccessor>;
  virtual ~SessionAccessor() {}
};

class SessionAccessorImpl : public SessionAccessor {
 public:
  explicit SessionAccessorImpl(scoped_ptr<Session> session);

  virtual Session* Access(scoped_ptr<base::AutoLock>* lock) OVERRIDE;

 private:
  virtual ~SessionAccessorImpl();

  base::Lock session_lock_;
  scoped_ptr<Session> session_;

  DISALLOW_COPY_AND_ASSIGN(SessionAccessorImpl);
};

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_H_
