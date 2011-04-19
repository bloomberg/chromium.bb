// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cookie_monster_store_test.h"

#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MockPersistentCookieStore::MockPersistentCookieStore()
    : load_return_value_(true) {
}

MockPersistentCookieStore::~MockPersistentCookieStore() {}

void MockPersistentCookieStore::SetLoadExpectation(
    bool return_value,
    const std::vector<CookieMonster::CanonicalCookie*>& result) {
  load_return_value_ = return_value;
  load_result_ = result;
}

bool MockPersistentCookieStore::Load(
    std::vector<CookieMonster::CanonicalCookie*>* out_cookies) {
  bool ok = load_return_value_;
  if (ok)
    *out_cookies = load_result_;
  return ok;
}

void MockPersistentCookieStore::AddCookie(
    const CookieMonster::CanonicalCookie& cookie) {
  commands_.push_back(
      CookieStoreCommand(CookieStoreCommand::ADD, cookie));
}

void MockPersistentCookieStore::UpdateCookieAccessTime(
    const CookieMonster::CanonicalCookie& cookie) {
  commands_.push_back(CookieStoreCommand(
      CookieStoreCommand::UPDATE_ACCESS_TIME, cookie));
}

void MockPersistentCookieStore::DeleteCookie(
    const CookieMonster::CanonicalCookie& cookie) {
  commands_.push_back(
      CookieStoreCommand(CookieStoreCommand::REMOVE, cookie));
}

void MockPersistentCookieStore::Flush(Task* completion_task) {
  if (completion_task)
    MessageLoop::current()->PostTask(FROM_HERE, completion_task);
}

// No files are created so nothing to clear either
void
MockPersistentCookieStore::SetClearLocalStateOnExit(bool clear_local_state) {
}

MockCookieMonsterDelegate::MockCookieMonsterDelegate() {}

void MockCookieMonsterDelegate::OnCookieChanged(
    const CookieMonster::CanonicalCookie& cookie,
    bool removed,
    CookieMonster::Delegate::ChangeCause cause) {
  CookieNotification notification(cookie, removed);
  changes_.push_back(notification);
}

MockCookieMonsterDelegate::~MockCookieMonsterDelegate() {}

void AddCookieToList(
    const std::string& key,
    const std::string& cookie_line,
    const base::Time& creation_time,
    std::vector<CookieMonster::CanonicalCookie*>* out_list) {

  // Parse the cookie line.
  CookieMonster::ParsedCookie pc(cookie_line);
  EXPECT_TRUE(pc.IsValid());

  // This helper is simplistic in interpreting a parsed cookie, in order to
  // avoid duplicated CookieMonster's CanonPath() and CanonExpiration()
  // functions. Would be nice to export them, and re-use here.
  EXPECT_FALSE(pc.HasMaxAge());
  EXPECT_TRUE(pc.HasPath());
  base::Time cookie_expires = pc.HasExpires() ?
      CookieMonster::ParseCookieTime(pc.Expires()) : base::Time();
  std::string cookie_path = pc.Path();

  scoped_ptr<CookieMonster::CanonicalCookie> cookie(
      new CookieMonster::CanonicalCookie(
          GURL(), pc.Name(), pc.Value(), key, cookie_path,
          creation_time, creation_time, cookie_expires,
          pc.IsSecure(), pc.IsHttpOnly(),
          !cookie_expires.is_null()));

  out_list->push_back(cookie.release());
}

MockSimplePersistentCookieStore::MockSimplePersistentCookieStore() {}

MockSimplePersistentCookieStore::~MockSimplePersistentCookieStore() {}

bool MockSimplePersistentCookieStore::Load(
    std::vector<CookieMonster::CanonicalCookie*>* out_cookies) {
  for (CanonicalCookieMap::const_iterator it = cookies_.begin();
       it != cookies_.end(); it++)
    out_cookies->push_back(
        new CookieMonster::CanonicalCookie(it->second));
  return true;
}

void MockSimplePersistentCookieStore::AddCookie(
    const CookieMonster::CanonicalCookie& cookie) {
  int64 creation_time = cookie.CreationDate().ToInternalValue();
  EXPECT_TRUE(cookies_.find(creation_time) == cookies_.end());
  cookies_[creation_time] = cookie;
}

void MockSimplePersistentCookieStore::UpdateCookieAccessTime(
    const CookieMonster::CanonicalCookie& cookie) {
  int64 creation_time = cookie.CreationDate().ToInternalValue();
  ASSERT_TRUE(cookies_.find(creation_time) != cookies_.end());
  cookies_[creation_time].SetLastAccessDate(base::Time::Now());
}

void MockSimplePersistentCookieStore::DeleteCookie(
    const CookieMonster::CanonicalCookie& cookie) {
  int64 creation_time = cookie.CreationDate().ToInternalValue();
  CanonicalCookieMap::iterator it = cookies_.find(creation_time);
  ASSERT_TRUE(it != cookies_.end());
  cookies_.erase(it);
}

void MockSimplePersistentCookieStore::Flush(Task* completion_task) {
  if (completion_task)
    MessageLoop::current()->PostTask(FROM_HERE, completion_task);
}

void MockSimplePersistentCookieStore::SetClearLocalStateOnExit(
    bool clear_local_state) {
}

CookieMonster* CreateMonsterFromStoreForGC(
    int num_cookies,
    int num_old_cookies,
    int days_old) {
  base::Time current(base::Time::Now());
  base::Time past_creation(base::Time::Now() - base::TimeDelta::FromDays(1000));
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  // Must expire to be persistent
  for (int i = 0; i < num_cookies; i++) {
    base::Time creation_time =
        past_creation + base::TimeDelta::FromMicroseconds(i);
    base::Time expiration_time = current + base::TimeDelta::FromDays(30);
    base::Time last_access_time =
        (i < num_old_cookies) ? current - base::TimeDelta::FromDays(days_old) :
                                current;

    CookieMonster::CanonicalCookie cc(
        GURL(), "a", "1", base::StringPrintf("h%05d.izzle", i), "/path",
        creation_time, expiration_time, last_access_time,
        false, false, true);
    store->AddCookie(cc);
  }

  return new CookieMonster(store, NULL);
}

}  // namespace net
