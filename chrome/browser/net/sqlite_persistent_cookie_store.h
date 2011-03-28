// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A sqlite implementation of a cookie monster persistent store.

#ifndef CHROME_BROWSER_NET_SQLITE_PERSISTENT_COOKIE_STORE_H_
#define CHROME_BROWSER_NET_SQLITE_PERSISTENT_COOKIE_STORE_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/cookie_monster.h"

class FilePath;

// Implements the PersistentCookieStore interface in terms of a SQLite database.
// For documentation about the actual member functions consult the documentation
// of the parent class |net::CookieMonster::PersistentCookieStore|.
class SQLitePersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  explicit SQLitePersistentCookieStore(const FilePath& path);
  virtual ~SQLitePersistentCookieStore();

  virtual bool Load(std::vector<net::CookieMonster::CanonicalCookie*>* cookies);

  virtual void AddCookie(const net::CookieMonster::CanonicalCookie& cc);
  virtual void UpdateCookieAccessTime(
      const net::CookieMonster::CanonicalCookie& cc);
  virtual void DeleteCookie(const net::CookieMonster::CanonicalCookie& cc);

  virtual void SetClearLocalStateOnExit(bool clear_local_state);

  virtual void Flush(Task* completion_task);

 private:
  class Backend;

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(SQLitePersistentCookieStore);
};

#endif  // CHROME_BROWSER_NET_SQLITE_PERSISTENT_COOKIE_STORE_H_
