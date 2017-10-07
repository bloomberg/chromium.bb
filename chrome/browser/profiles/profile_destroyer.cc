// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"

namespace {

#if defined(OS_ANDROID)
// Set the render host waiting time to 5s on Android, that's the same
// as an "Application Not Responding" timeout.
const int64_t kTimerDelaySeconds = 5;
#else
const int64_t kTimerDelaySeconds = 1;
#endif

}  // namespace

ProfileDestroyer::DestroyerSet* ProfileDestroyer::pending_destroyers_ = NULL;

// static
void ProfileDestroyer::DestroyProfileWhenAppropriate(Profile* const profile) {
  TRACE_EVENT0("shutdown", "ProfileDestroyer::DestroyProfileWhenAppropriate");

  DCHECK(profile);
  profile->MaybeSendDestroyedNotification();

  HostSet hosts;
  // Testing profiles can simply be deleted directly. Some tests don't setup
  // RenderProcessHost correctly and don't necessary run on the UI thread
  // anyway, so we can't use the AllHostIterator.
  if (profile->AsTestingProfile() == NULL) {
    GetHostsForProfile(profile, &hosts);
    if (!profile->IsOffTheRecord() && profile->HasOffTheRecordProfile())
      GetHostsForProfile(profile->GetOffTheRecordProfile(), &hosts);
  }
  // Generally, !hosts.empty() means that there is a leak in a render process
  // host that MUST BE FIXED!!!
  //
  // However, off-the-record profiles are destroyed before their
  // RenderProcessHosts in order to erase private data quickly, and
  // RenderProcessHostImpl::Release() avoids destroying RenderProcessHosts in
  // --single-process mode to avoid race conditions.
  DCHECK(hosts.empty() || profile->IsOffTheRecord() ||
         profile->HasOffTheRecordProfile() ||
         content::RenderProcessHost::run_renderer_in_process())
      << "Profile still has " << hosts.size() << " hosts";
  // Note that we still test for !profile->IsOffTheRecord here even though we
  // DCHECK'd above because we want to protect Release builds against this even
  // we need to identify if there are leaks when we run Debug builds.
  if (hosts.empty() || !profile->IsOffTheRecord()) {
    if (profile->IsOffTheRecord())
      profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
    else
      delete profile;
  } else {
    // The instance will destroy itself once all render process hosts referring
    // to it are properly terminated.
    new ProfileDestroyer(profile, &hosts);
  }
}

// This can be called to cancel any pending destruction and destroy the profile
// now, e.g., if the parent profile is being destroyed while the incognito one
// still pending...
void ProfileDestroyer::DestroyOffTheRecordProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
  if (pending_destroyers_) {
    for (DestroyerSet::iterator i = pending_destroyers_->begin();
         i != pending_destroyers_->end(); ++i) {
      if ((*i)->profile_ == profile) {
        // We want to signal this in debug builds so that we don't lose sight of
        // these potential leaks, but we handle it in release so that we don't
        // crash or corrupt profile data on disk.
        NOTREACHED() << "A render process host wasn't destroyed early enough.";
        (*i)->profile_ = NULL;
        break;
      }
    }
  }
  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
}

ProfileDestroyer::ProfileDestroyer(Profile* const profile, HostSet* hosts)
    : timer_(false, false),
      num_hosts_(0),
      profile_(profile),
      weak_ptr_factory_(this) {
  if (pending_destroyers_ == NULL)
    pending_destroyers_ = new DestroyerSet;
  pending_destroyers_->insert(this);
  for (HostSet::iterator i = hosts->begin(); i != hosts->end(); ++i) {
    (*i)->AddObserver(this);
    // For each of the observations, we bump up our reference count.
    // It will go back to 0 and free us when all hosts are terminated.
    ++num_hosts_;
  }
  // If we are going to wait for render process hosts, we don't want to do it
  // for longer than kTimerDelaySeconds.
  if (num_hosts_) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kTimerDelaySeconds),
                 base::Bind(&ProfileDestroyer::DestroyProfile,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

ProfileDestroyer::~ProfileDestroyer() {
  // Check again, in case other render hosts were added while we were
  // waiting for the previous ones to go away...
  if (profile_)
    DestroyProfileWhenAppropriate(profile_);

#ifdef NDEBUG
  // Don't wait for pending registrations, if any, these hosts are buggy.
  // Note: this can happen, but if so, it's better to crash here than wait
  // for the host to dereference a deleted Profile. http://crbug.com/248625
  CHECK_EQ(0U, num_hosts_) << "Some render process hosts were not "
                           << "destroyed early enough!";
#endif  // NDEBUG
  DCHECK(pending_destroyers_ != NULL);
  DestroyerSet::iterator iter = pending_destroyers_->find(this);
  DCHECK(iter != pending_destroyers_->end());
  pending_destroyers_->erase(iter);
  if (pending_destroyers_->empty()) {
    delete pending_destroyers_;
    pending_destroyers_ = NULL;
  }
}

void ProfileDestroyer::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK(num_hosts_ > 0);
  --num_hosts_;
  if (num_hosts_ == 0) {
    // Delay the destruction one step further in case other observers need to
    // look at the profile attached to the host.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ProfileDestroyer::DestroyProfile,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProfileDestroyer::DestroyProfile() {
  // We might have been cancelled externally before the timer expired.
  if (!profile_) {
    delete this;
    return;
  }

  DCHECK(profile_->IsOffTheRecord());
  DCHECK(profile_->GetOriginalProfile());
  profile_->GetOriginalProfile()->DestroyOffTheRecordProfile();
  profile_ = NULL;

  // And stop the timer so we can be released early too.
  timer_.Stop();

  delete this;
}

// static
bool ProfileDestroyer::GetHostsForProfile(
    Profile* const profile, HostSet* hosts) {
  for (content::RenderProcessHost::iterator iter(
        content::RenderProcessHost::AllHostsIterator());
      !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    if (render_process_host &&
            render_process_host->GetBrowserContext() == profile) {
      hosts->insert(render_process_host);
    }
  }
  return !hosts->empty();
}
