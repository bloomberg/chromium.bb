// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/fake_session_accessor.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/session_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

Status ExecuteSimpleCommand(
    Session* expected_session,
    base::DictionaryValue* expected_params,
    base::Value* value,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* return_value) {
  EXPECT_EQ(expected_session, session);
  EXPECT_TRUE(expected_params->Equals(&params));
  return_value->reset(value->DeepCopy());
  return Status(kOk);
}

}  // namespace

TEST(SessionCommandTest, SimpleCommand) {
  SessionMap map;
  Session session("session", scoped_ptr<Chrome>(new StubChrome()));
  ASSERT_TRUE(session.thread.Start());
  scoped_refptr<SessionAccessor> accessor(new FakeSessionAccessor(&session));
  map.Set(session.id, accessor);

  base::DictionaryValue params;
  params.SetInteger("param", 5);
  base::FundamentalValue expected_value(6);
  SessionCommand cmd = base::Bind(
      &ExecuteSimpleCommand, &session, &params, &expected_value);

  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status = ExecuteSessionCommand(
      &map, cmd, params,
      session.id, &value, &session_id);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(expected_value.Equals(value.get()));
  ASSERT_STREQ(session.id.c_str(), session_id.c_str());
}

namespace {

Status ShouldNotBeCalled(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  EXPECT_TRUE(false);
  return Status(kOk);
}

}  // namespace

TEST(SessionCommandTest, NoSuchSession) {
  SessionMap map;
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status = ExecuteSessionCommand(
      &map, base::Bind(&ShouldNotBeCalled), params,
      "session", &value, &session_id);
  ASSERT_EQ(kNoSuchSession, status.code());
  ASSERT_FALSE(value.get());
  ASSERT_STREQ("session", session_id.c_str());
}

TEST(SessionCommandTest, SessionDeletedWhileWaiting) {
  SessionMap map;
  scoped_refptr<SessionAccessor> accessor(new FakeSessionAccessor(NULL));
  map.Set("session", accessor);

  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status = ExecuteSessionCommand(
      &map, base::Bind(&ShouldNotBeCalled), params,
      "session", &value, &session_id);
  ASSERT_EQ(kNoSuchSession, status.code());
  ASSERT_FALSE(value.get());
  ASSERT_STREQ("session", session_id.c_str());
}
