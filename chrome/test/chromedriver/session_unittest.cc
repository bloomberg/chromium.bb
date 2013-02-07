// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/stub_chrome.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ValidFrameChrome : public StubChrome {
 public:
  ValidFrameChrome() {}
  virtual ~ValidFrameChrome() {}

  virtual Status GetMainFrame(std::string* frame_id) OVERRIDE {
    *frame_id = "main";
    return Status(kOk);
  }

  virtual Status WaitForPendingNavigations(
      const std::string& frame_id) OVERRIDE {
    if (frame_id == "")
      return Status(kUnknownError, "frame_id was empty");
    return Status(kOk);
  }
};

}  // namespace

TEST(SessionAccessorTest, LocksSession) {
  scoped_ptr<Session> scoped_session(new Session("id"));
  Session* session = scoped_session.get();
  scoped_refptr<SessionAccessor> accessor(
      new SessionAccessorImpl(scoped_session.Pass()));
  scoped_ptr<base::AutoLock> lock;
  ASSERT_EQ(session, accessor->Access(&lock));
  ASSERT_TRUE(lock.get());
}

TEST(Session, WaitForPendingNavigationsNoChrome) {
  Session session("id");
  ASSERT_TRUE(session.WaitForPendingNavigations().IsOk());
}

TEST(Session, WaitForPendingNavigations) {
  scoped_ptr<Chrome> chrome(new ValidFrameChrome());
  Session session("id", chrome.Pass());
  session.frame = "f";
  ASSERT_TRUE(session.WaitForPendingNavigations().IsOk());
}

TEST(Session, WaitForPendingNavigationsMainFrame) {
  scoped_ptr<Chrome> chrome(new ValidFrameChrome());
  Session session("id", chrome.Pass());
  session.frame = "";
  ASSERT_TRUE(session.WaitForPendingNavigations().IsOk());
}
