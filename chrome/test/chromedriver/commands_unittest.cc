// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/fake_session_accessor.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class StubChrome : public Chrome {
 public:
  StubChrome() {}
  virtual ~StubChrome() {}

  // Overridden from Chrome:
  virtual Status Load(const std::string& url) OVERRIDE {
    return Status(kOk);
  }
  virtual Status Quit() OVERRIDE {
    return Status(kOk);
  }
};

class OkLauncher : public ChromeLauncher {
 public:
  OkLauncher() {}
  virtual ~OkLauncher() {}

  // Overridden from ChromeLauncher:
  virtual Status Launch(const FilePath& chrome_exe,
                        scoped_ptr<Chrome>* chrome) OVERRIDE {
    chrome->reset(new StubChrome());
    return Status(kOk);
  }
};

}  // namespace

TEST(CommandsTest, NewSession) {
  OkLauncher launcher;
  SessionMap map;
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteNewSession(&map, &launcher, params, "", &value, &session_id);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(value);
  std::string id;
  ASSERT_TRUE(value->GetAsString(&id));
  ASSERT_EQ(32u, id.length());
  ASSERT_STREQ(id.c_str(), session_id.c_str());
  scoped_refptr<SessionAccessor> accessor;
  ASSERT_TRUE(map.Get(id, &accessor));
  scoped_ptr<base::AutoLock> lock;
  Session* session = accessor->Access(&lock);
  ASSERT_TRUE(session);
  ASSERT_STREQ(id.c_str(), session->id.c_str());
  ASSERT_TRUE(session->chrome);
}

namespace {

class FailLauncher : public ChromeLauncher {
 public:
  FailLauncher() {}
  virtual ~FailLauncher() {}

  // Overridden from ChromeLauncher:
  virtual Status Launch(const FilePath& chrome_exe,
                        scoped_ptr<Chrome>* chrome) OVERRIDE {
    return Status(kUnknownError);
  }
};

}  // namespace

TEST(CommandsTest, NewSessionLauncherFails) {
  FailLauncher launcher;
  SessionMap map;
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteNewSession(&map, &launcher, params, "", &value, &session_id);
  ASSERT_EQ(kSessionNotCreatedException, status.code());
  ASSERT_FALSE(value);
}

namespace {

Status ExecuteStubQuit(
    int* count,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  if (*count == 0) {
    EXPECT_STREQ("id", session_id.c_str());
  } else {
    EXPECT_STREQ("id2", session_id.c_str());
  }
  (*count)++;
  return Status(kOk);
}

}  // namespace

TEST(CommandsTest, QuitAll) {
  SessionMap map;
  Session session("id");
  Session session2("id2");
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  map.Set(session2.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session2)));

  int count = 0;
  Command cmd = base::Bind(&ExecuteStubQuit, &count);
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteQuitAll(cmd, &map, params, "", &value, &session_id);
  ASSERT_EQ(kOk, status.code());
  ASSERT_FALSE(value.get());
  ASSERT_EQ(2, count);
}

TEST(CommandsTest, Quit) {
  SessionMap map;
  Session session("id", scoped_ptr<Chrome>(new StubChrome()));
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kOk, ExecuteQuit(&map, &session, params, &value).code());
  ASSERT_FALSE(map.Has(session.id));
  ASSERT_FALSE(value.get());
}

namespace {

class FailsToQuitChrome : public StubChrome {
 public:
  FailsToQuitChrome() {}
  virtual ~FailsToQuitChrome() {}

  // Overridden from Chrome:
  virtual Status Quit() OVERRIDE {
    return Status(kUnknownError);
  }
};

}  // namespace

TEST(CommandsTest, QuitFails) {
  SessionMap map;
  Session session("id", scoped_ptr<Chrome>(new FailsToQuitChrome()));
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError, ExecuteQuit(&map, &session, params, &value).code());
  ASSERT_FALSE(map.Has(session.id));
  ASSERT_FALSE(value.get());
}
