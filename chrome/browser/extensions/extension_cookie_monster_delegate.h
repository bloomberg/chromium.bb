// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_MONSTER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_MONSTER_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "net/cookies/cookie_monster.h"

class Profile;

// Sends cookie-change notifications from the UI thread via
// chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS.
class ExtensionCookieMonsterDelegate : public net::CookieMonsterDelegate {
 public:
  explicit ExtensionCookieMonsterDelegate(Profile* profile);

  // net::CookieMonsterDelegate implementation.
  void OnCookieChanged(
      const net::CanonicalCookie& cookie,
      bool removed,
      net::CookieStore::ChangeCause cause) override;

 private:
  ~ExtensionCookieMonsterDelegate() override;

  void OnCookieChangedAsyncHelper(
      const net::CanonicalCookie& cookie,
      bool removed,
      net::CookieStore::ChangeCause cause);

  const base::Callback<Profile*(void)> profile_getter_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCookieMonsterDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_MONSTER_DELEGATE_H_
