// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_change_dispatcher.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"

namespace net {

namespace {

// This class owns the CookieChangeCallbackList::Subscription,
// thus guaranteeing destruction when it is destroyed.  In addition, it
// wraps the callback for a particular subscription, guaranteeing that it
// won't be run even if a PostTask completes after the subscription has
// been destroyed.
class CookieMonsterChangeSubscription : public CookieChangeSubscription {
 public:
  using CookieChangeCallbackList =
      CookieMonsterChangeDispatcher::CookieChangeCallbackList;

  CookieMonsterChangeSubscription(const CookieChangeCallback& callback)
      : callback_(callback), weak_ptr_factory_(this) {}
  ~CookieMonsterChangeSubscription() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  void SetCallbackSubscription(
      std::unique_ptr<CookieChangeCallbackList::Subscription> subscription) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    subscription_ = std::move(subscription);
  }

  // The returned callback runs the callback passed to the constructor
  // directly as long as this object hasn't been destroyed.
  CookieChangeCallback WeakCallback() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    return base::BindRepeating(&CookieMonsterChangeSubscription::DispatchChange,
                               weak_ptr_factory_.GetWeakPtr());
  }

 private:
  void DispatchChange(const CanonicalCookie& cookie, CookieChangeCause cause) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    callback_.Run(cookie, cause);
  }

  const CookieChangeCallback callback_;
  std::unique_ptr<CookieChangeCallbackList::Subscription> subscription_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CookieMonsterChangeSubscription> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonsterChangeSubscription);
};

void RunAsync(scoped_refptr<base::TaskRunner> proxy,
              const CookieChangeCallback& callback,
              const CanonicalCookie& cookie,
              CookieChangeCause cause) {
  proxy->PostTask(FROM_HERE, base::BindRepeating(callback, cookie, cause));
}

}  // anonymous namespace

CookieMonsterChangeDispatcher::CookieMonsterChangeDispatcher() = default;
CookieMonsterChangeDispatcher::~CookieMonsterChangeDispatcher() = default;

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForCookie(
    const GURL& gurl,
    const std::string& name,
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::pair<GURL, std::string> key(gurl, name);
  if (hook_map_.count(key) == 0)
    hook_map_[key] = std::make_unique<CookieChangeCallbackList>();

  auto subscription =
      std::make_unique<CookieMonsterChangeSubscription>(std::move(callback));
  subscription->SetCallbackSubscription(hook_map_[key]->Add(
      base::BindRepeating(&RunAsync, base::ThreadTaskRunnerHandle::Get(),
                          subscription->WeakCallback())));

  return std::move(subscription);
}

std::unique_ptr<CookieChangeSubscription>
CookieMonsterChangeDispatcher::AddCallbackForAllChanges(
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto subscription =
      std::make_unique<CookieMonsterChangeSubscription>(std::move(callback));
  subscription->SetCallbackSubscription(global_hook_map_.Add(
      base::BindRepeating(&RunAsync, base::ThreadTaskRunnerHandle::Get(),
                          subscription->WeakCallback())));
  return std::move(subscription);
}

void CookieMonsterChangeDispatcher::DispatchChange(
    const CanonicalCookie& cookie,
    CookieChangeCause cause,
    bool notify_global_hooks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CookieOptions opts;
  opts.set_include_httponly();
  opts.set_same_site_cookie_mode(
      CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  // Note that the callbacks in hook_map_ are wrapped with RunAsync(), so they
  // are guaranteed to not take long - they just post a RunAsync task back to
  // the appropriate thread's message loop and return.
  // TODO(mmenke): Consider running these synchronously?
  for (const auto& key_value_pair : hook_map_) {
    const std::pair<GURL, std::string>& key = key_value_pair.first;
    if (cookie.IncludeForRequestURL(key.first, opts) &&
        cookie.Name() == key.second) {
      key_value_pair.second->Notify(cookie, cause);
    }
  }

  if (notify_global_hooks)
    global_hook_map_.Notify(cookie, cause);
}

}  // namespace net
