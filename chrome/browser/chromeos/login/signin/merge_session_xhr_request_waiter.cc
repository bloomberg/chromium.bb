// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/merge_session_xhr_request_waiter.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Maximum time for delaying XHR requests.
const int kMaxRequestWaitTimeMS = 10000;

}

MergeSessionXHRRequestWaiter::MergeSessionXHRRequestWaiter(
    Profile* profile,
    const MergeSessionThrottle::CompletionCallback& callback)
   : profile_(profile),
     callback_(callback),
     weak_ptr_factory_(this) {
}

MergeSessionXHRRequestWaiter::~MergeSessionXHRRequestWaiter() {
  chromeos::OAuth2LoginManager* manager =
      chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
          profile_);
  if (manager)
    manager->RemoveObserver(this);
}

void MergeSessionXHRRequestWaiter::StartWaiting() {
  OAuth2LoginManager* manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
  if (manager && manager->ShouldBlockTabLoading()) {
    DVLOG(1) << "Waiting for XHR request throttle";
    manager->AddObserver(this);
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MergeSessionXHRRequestWaiter::OnTimeout,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kMaxRequestWaitTimeMS));
  } else {
    NotifyBlockingDone();
  }
}

void MergeSessionXHRRequestWaiter::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  OAuth2LoginManager* manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
  DVLOG(1) << "Merge session throttle should "
           << (!manager->ShouldBlockTabLoading() ?
                  " NOT" : "")
           << " be blocking now, "
           << state;
  if (!manager->ShouldBlockTabLoading()) {
    DVLOG(1) << "Unblocking XHR request throttle due to session merge";
    manager->RemoveObserver(this);
    NotifyBlockingDone();
  }
}

void MergeSessionXHRRequestWaiter::OnTimeout() {
  DVLOG(1) << "Unblocking XHR request throttle due to timeout";
  NotifyBlockingDone();
}

void MergeSessionXHRRequestWaiter::NotifyBlockingDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback_.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, callback_);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace chromeos
