// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains test infrastructure for multiple files
// (current cookie_monster_unittest.cc and cookie_monster_perftest.cc)
// that need to test out CookieMonster interactions with the backing store.
// It should only be included by test code.

#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Describes a call to one of the 3 functions of PersistentCookieStore.
struct CookieStoreCommand {
  enum Type {
    ADD,
    UPDATE_ACCESS_TIME,
    REMOVE,
  };

  CookieStoreCommand(Type type,
                     const net::CookieMonster::CanonicalCookie& cookie)
      : type(type),
        cookie(cookie) {}

  Type type;
  net::CookieMonster::CanonicalCookie cookie;
};

// Implementation of PersistentCookieStore that captures the
// received commands and saves them to a list.
// The result of calls to Load() can be configured using SetLoadExpectation().
class MockPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  typedef std::vector<CookieStoreCommand> CommandList;

  MockPersistentCookieStore() : load_return_value_(true) {
  }

  virtual bool Load(
      std::vector<net::CookieMonster::CanonicalCookie*>* out_cookies) {
    bool ok = load_return_value_;
    if (ok)
      *out_cookies = load_result_;
    return ok;
  }

  virtual void AddCookie(const net::CookieMonster::CanonicalCookie& cookie) {
    commands_.push_back(
        CookieStoreCommand(CookieStoreCommand::ADD, cookie));
  }

  virtual void UpdateCookieAccessTime(
      const net::CookieMonster::CanonicalCookie& cookie) {
    commands_.push_back(CookieStoreCommand(
        CookieStoreCommand::UPDATE_ACCESS_TIME, cookie));
  }

  virtual void DeleteCookie(
      const net::CookieMonster::CanonicalCookie& cookie) {
    commands_.push_back(
        CookieStoreCommand(CookieStoreCommand::REMOVE, cookie));
  }

  virtual void Flush(Task* completion_task) {
    if (completion_task)
      MessageLoop::current()->PostTask(FROM_HERE, completion_task);
  }

  // No files are created so nothing to clear either
  virtual void SetClearLocalStateOnExit(bool clear_local_state) {}

  void SetLoadExpectation(
      bool return_value,
      const std::vector<net::CookieMonster::CanonicalCookie*>& result) {
    load_return_value_ = return_value;
    load_result_ = result;
  }

  const CommandList& commands() const {
    return commands_;
  }

 private:
  CommandList commands_;

  // Deferred result to use when Load() is called.
  bool load_return_value_;
  std::vector<net::CookieMonster::CanonicalCookie*> load_result_;

  DISALLOW_COPY_AND_ASSIGN(MockPersistentCookieStore);
};

// Mock for CookieMonster::Delegate
class MockCookieMonsterDelegate : public net::CookieMonster::Delegate {
 public:
  typedef std::pair<net::CookieMonster::CanonicalCookie, bool>
      CookieNotification;

  MockCookieMonsterDelegate() {}

  virtual void OnCookieChanged(
      const net::CookieMonster::CanonicalCookie& cookie,
      bool removed) {
    CookieNotification notification(cookie, removed);
    changes_.push_back(notification);
  }

  const std::vector<CookieNotification>& changes() const { return changes_; }

  void reset() { changes_.clear(); }

 private:
  virtual ~MockCookieMonsterDelegate() {}

  std::vector<CookieNotification> changes_;

  DISALLOW_COPY_AND_ASSIGN(MockCookieMonsterDelegate);
};

// Helper to build a list of CanonicalCookie*s.
static void AddCookieToList(
    const std::string& key,
    const std::string& cookie_line,
    const base::Time& creation_time,
    std::vector<net::CookieMonster::CanonicalCookie*>* out_list) {

  // Parse the cookie line.
  net::CookieMonster::ParsedCookie pc(cookie_line);
  EXPECT_TRUE(pc.IsValid());

  // This helper is simplistic in interpreting a parsed cookie, in order to
  // avoid duplicated CookieMonster's CanonPath() and CanonExpiration()
  // functions. Would be nice to export them, and re-use here.
  EXPECT_FALSE(pc.HasMaxAge());
  EXPECT_TRUE(pc.HasPath());
  base::Time cookie_expires = pc.HasExpires() ?
      net::CookieMonster::ParseCookieTime(pc.Expires()) : base::Time();
  std::string cookie_path = pc.Path();

  scoped_ptr<net::CookieMonster::CanonicalCookie> cookie(
      new net::CookieMonster::CanonicalCookie(
          pc.Name(), pc.Value(), key, cookie_path,
          pc.IsSecure(), pc.IsHttpOnly(),
          creation_time, creation_time,
          !cookie_expires.is_null(),
          cookie_expires));

  out_list->push_back(cookie.release());
}

// Just act like a backing database.  Keep cookie information from
// Add/Update/Delete and regurgitate it when Load is called.
class MockSimplePersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 private:
  typedef std::map<int64, net::CookieMonster::CanonicalCookie>
      CanonicalCookieMap;

 public:
  virtual bool Load(
      std::vector<net::CookieMonster::CanonicalCookie*>* out_cookies) {
    for (CanonicalCookieMap::const_iterator it = cookies_.begin();
         it != cookies_.end(); it++)
      out_cookies->push_back(
          new net::CookieMonster::CanonicalCookie(it->second));
    return true;
  }

  virtual void AddCookie(
      const net::CookieMonster::CanonicalCookie& cookie) {
    int64 creation_time = cookie.CreationDate().ToInternalValue();
    EXPECT_TRUE(cookies_.find(creation_time) == cookies_.end());
    cookies_[creation_time] = cookie;
  }

  virtual void UpdateCookieAccessTime(
      const net::CookieMonster::CanonicalCookie& cookie) {
    int64 creation_time = cookie.CreationDate().ToInternalValue();
    ASSERT_TRUE(cookies_.find(creation_time) != cookies_.end());
    cookies_[creation_time].SetLastAccessDate(base::Time::Now());
  }

  virtual void DeleteCookie(
      const net::CookieMonster::CanonicalCookie& cookie) {
    int64 creation_time = cookie.CreationDate().ToInternalValue();
    CanonicalCookieMap::iterator it = cookies_.find(creation_time);
    ASSERT_TRUE(it != cookies_.end());
    cookies_.erase(it);
  }

  virtual void Flush(Task* completion_task) {
    if (completion_task)
      MessageLoop::current()->PostTask(FROM_HERE, completion_task);
  }

  virtual void SetClearLocalStateOnExit(bool clear_local_state) {}

 private:
  CanonicalCookieMap cookies_;
};

// Helper function for creating a CookieMonster backed by a
// MockSimplePersistentCookieStore for garbage collection testing.
//
// Fill the store through import with |num_cookies| cookies, |num_old_cookies|
// with access time Now()-days_old, the rest with access time Now().
// Do two SetCookies().  Return whether each of the two SetCookies() took
// longer than |gc_perf_micros| to complete, and how many cookie were
// left in the store afterwards.
static net::CookieMonster* CreateMonsterFromStoreForGC(
    int num_cookies,
    int num_old_cookies,
    int days_old) {
  base::Time current(base::Time::Now());
  base::Time past_creation(base::Time::Now() - base::TimeDelta::FromDays(1000));
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  // Must expire to be persistent
  for (int i = 0; i < num_old_cookies; i++) {
    net::CookieMonster::CanonicalCookie cc(
        "a", "1", StringPrintf("h%05d.izzle", i), "/path", false, false,
        past_creation + base::TimeDelta::FromMicroseconds(i),
        current - base::TimeDelta::FromDays(days_old),
        true, current + base::TimeDelta::FromDays(30));
    store->AddCookie(cc);
  }
  for (int i = num_old_cookies; i < num_cookies; i++) {
    net::CookieMonster::CanonicalCookie cc(
        "a", "1", StringPrintf("h%05d.izzle", i), "/path", false, false,
        past_creation + base::TimeDelta::FromMicroseconds(i), current,
        true, current + base::TimeDelta::FromDays(30));
    store->AddCookie(cc);
  }

  return new net::CookieMonster(store, NULL);
}

}  // namespace
