// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_MONSTER_CHANGE_DISPATCHER_H_
#define NET_COOKIES_COOKIE_MONSTER_CHANGE_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "url/gurl.h"

namespace net {

class CanonicalCookie;

// CookieChangeDispatcher implementation used by CookieMonster.
class CookieMonsterChangeDispatcher : public CookieChangeDispatcher {
 public:
  using CookieChangeCallbackList =
      base::CallbackList<void(const CanonicalCookie& cookie,
                              CookieChangeCause cause)>;

  CookieMonsterChangeDispatcher();
  ~CookieMonsterChangeDispatcher() override;

  // net::CookieChangeDispatcher
  std::unique_ptr<CookieChangeSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      CookieChangeCallback callback) override WARN_UNUSED_RESULT;
  std::unique_ptr<CookieChangeSubscription> AddCallbackForAllChanges(
      CookieChangeCallback callback) override WARN_UNUSED_RESULT;

  // Dispatch a cookie change to all interested listeners.
  //
  // |notify_global_hooks| is true if the function should run the
  // global hooks in addition to the per-cookie hooks.
  // TODO(pwnall): Remove |notify_global_hooks|.
  void DispatchChange(const CanonicalCookie& cookie,
                      CookieChangeCause cause,
                      bool notify_global_hooks);

 private:
  using CookieChangeHookMap =
      std::map<std::pair<GURL, std::string>,
               std::unique_ptr<CookieChangeCallbackList>>;
  CookieChangeHookMap hook_map_;
  CookieChangeCallbackList global_hook_map_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CookieMonsterChangeDispatcher);
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_CHANGE_DISPATCHER_H_
