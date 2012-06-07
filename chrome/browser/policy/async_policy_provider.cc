// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/async_policy_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/policy/async_policy_loader.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace policy {

namespace {

// Helper for a PostTaskAndReply used as a synchronization point between the
// main thread and FILE thread. See AsyncPolicyProvider::RefreshPolicies.
void Nop() {}

}  // namespace

AsyncPolicyProvider::AsyncPolicyProvider(scoped_ptr<AsyncPolicyLoader> loader)
    : loader_(loader.release()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // The FILE thread isn't ready early during startup. Post a task to the
  // current loop to resume initialization on FILE once the loops are spinning.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AsyncPolicyProvider::InitWithLoopsReady,
                 weak_factory_.GetWeakPtr()));
  // Make an immediate synchronous load on startup.
  OnLoaderReloaded(loader_->InitialLoad());
}

AsyncPolicyProvider::~AsyncPolicyProvider() {
  DCHECK(CalledOnValidThread());

  // Note on the lifetime of |loader_|:
  // The |loader_| lives on the FILE thread, and is deleted from here. This
  // means that posting tasks on the |loader_| to FILE from the
  // AsyncPolicyProvider is always safe, since a potential DeleteSoon() is only
  // posted from here. The |loader_| posts back to the AsyncPolicyProvider
  // through the |update_callback_|, which has a WeakPtr to |this|.
  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, loader_);
}

void AsyncPolicyProvider::RefreshPolicies() {
  DCHECK(CalledOnValidThread());

  // Subtle: RefreshPolicies() has a contract that requires the next policy
  // update notification (triggered from UpdatePolicy()) to reflect any changes
  // made before this call. So if a caller has modified the policy settings and
  // invoked RefreshPolicies(), then by the next notification these policies
  // should already be provided.
  // However, it's also possible that an asynchronous Reload() is in progress
  // and just posted OnLoaderReloaded(). Therefore a task is posted to the
  // FILE thread before posting the next Reload, to prevent a potential
  // concurrent Reload() from triggering a notification too early. If another
  // refresh task has been posted, it is invalidated now.
  refresh_callback_.Reset(
      base::Bind(&AsyncPolicyProvider::ReloadAfterRefreshSync,
                 base::Unretained(this)));
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(Nop),
      refresh_callback_.callback());
}

void AsyncPolicyProvider::InitWithLoopsReady() {
  DCHECK(CalledOnValidThread());
  AsyncPolicyLoader::UpdateCallback callback =
      base::Bind(&AsyncPolicyProvider::LoaderUpdateCallback,
                 base::MessageLoopProxy::current(),
                 weak_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AsyncPolicyLoader::Init,
                 base::Unretained(loader_),
                 callback));
}

void AsyncPolicyProvider::ReloadAfterRefreshSync() {
  DCHECK(CalledOnValidThread());
  // This task can only enter if it was posted from RefreshPolicies(), and it
  // hasn't been cancelled meanwhile by another call to RefreshPolicies().
  DCHECK(!refresh_callback_.IsCancelled());
  // There can't be another refresh callback pending now, since its creation
  // in RefreshPolicies() would have cancelled the current execution. So it's
  // safe to cancel the |refresh_callback_| now, so that OnLoaderReloaded()
  // sees that there is no refresh pending.
  refresh_callback_.Cancel();

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AsyncPolicyLoader::Reload,
                 base::Unretained(loader_),
                 true  /* force */));
}

void AsyncPolicyProvider::OnLoaderReloaded(scoped_ptr<PolicyBundle> bundle) {
  DCHECK(CalledOnValidThread());
  if (refresh_callback_.IsCancelled())
    UpdatePolicy(bundle.Pass());
}

// static
void AsyncPolicyProvider::LoaderUpdateCallback(
    scoped_refptr<base::MessageLoopProxy> loop,
    base::WeakPtr<AsyncPolicyProvider> weak_this,
    scoped_ptr<PolicyBundle> bundle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  loop->PostTask(FROM_HERE,
                 base::Bind(&AsyncPolicyProvider::OnLoaderReloaded,
                            weak_this,
                            base::Passed(&bundle)));
}

}  // namespace policy
