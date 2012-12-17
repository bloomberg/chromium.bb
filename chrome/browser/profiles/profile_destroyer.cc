// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"


namespace {

const int64 kTimerDelaySeconds = 1;

}  // namespace

std::vector<ProfileDestroyer*>* ProfileDestroyer::pending_destroyers_ = NULL;

// static
void ProfileDestroyer::DestroyProfileWhenAppropriate(Profile* const profile) {
  DCHECK(profile);
  profile->MaybeSendDestroyedNotification();

  std::vector<content::RenderProcessHost*> hosts;
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
         content::RenderProcessHost::run_renderer_in_process());
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
    scoped_refptr<ProfileDestroyer> profile_destroyer(
        new ProfileDestroyer(profile, hosts));
  }
}

// This can be called to cancel any pending destruction and destroy the profile
// now, e.g., if the parent profile is being destroyed while the incognito one
// still pending...
void ProfileDestroyer::DestroyOffTheRecordProfileNow(Profile* const profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());
  if (pending_destroyers_) {
    for (size_t i = 0; i < pending_destroyers_->size(); ++i) {
      if ((*pending_destroyers_)[i]->profile_ == profile) {
        // We want to signal this in debug builds so that we don't lose sight of
        // these potential leaks, but we handle it in release so that we don't
        // crash or corrupt profile data on disk.
        NOTREACHED() << "A render process host wasn't destroyed early enough.";
        (*pending_destroyers_)[i]->profile_ = NULL;
        break;
      }
    }
  }
  DCHECK(profile->GetOriginalProfile());
  profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
}

ProfileDestroyer::ProfileDestroyer(
    Profile* const profile,
    const std::vector<content::RenderProcessHost*>& hosts)
    : timer_(false, false),
      num_hosts_(0),
      profile_(profile) {
  if (pending_destroyers_ == NULL)
    pending_destroyers_ = new std::vector<ProfileDestroyer*>;
  pending_destroyers_->push_back(this);
  for (size_t i = 0; i < hosts.size(); ++i) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                   content::Source<content::RenderProcessHost>(hosts[i]));
    // For each of the notifications, we bump up our reference count.
    // It will go back to 0 and free us when all hosts are terminated.
    ++num_hosts_;
  }
  // If we are going to wait for render process hosts, we don't want to do it
  // for longer than kTimerDelaySeconds.
  if (num_hosts_) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kTimerDelaySeconds),
                 base::Bind(&ProfileDestroyer::DestroyProfile, this));
  }
}

ProfileDestroyer::~ProfileDestroyer() {
  // Check again, in case other render hosts were added while we were
  // waiting for the previous ones to go away...
  if (profile_)
    DestroyProfileWhenAppropriate(profile_);

  // We shouldn't be deleted with pending notifications.
  DCHECK(registrar_.IsEmpty());

  DCHECK(pending_destroyers_ != NULL);
  std::vector<ProfileDestroyer*>::iterator iter = std::find(
      pending_destroyers_->begin(), pending_destroyers_->end(), this);
  DCHECK(iter != pending_destroyers_->end());
  pending_destroyers_->erase(iter);
  DCHECK(pending_destroyers_->end() == std::find(pending_destroyers_->begin(),
                                                 pending_destroyers_->end(),
                                                 this));
  if (pending_destroyers_->empty()) {
    delete pending_destroyers_;
    pending_destroyers_ = NULL;
  }
}

void ProfileDestroyer::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                    source);
  DCHECK(num_hosts_ > 0);
  --num_hosts_;
  if (num_hosts_ == 0) {
    // Delay the destruction one step further in case other observers of this
    // notification need to look at the profile attached to the host.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ProfileDestroyer::DestroyProfile, this));
  }
}

void ProfileDestroyer::DestroyProfile() {
  // We might have been cancelled externally before the timer expired.
  if (profile_ == NULL)
    return;
  DCHECK(profile_->IsOffTheRecord());
  DCHECK(profile_->GetOriginalProfile());
  profile_->GetOriginalProfile()->DestroyOffTheRecordProfile();
  profile_ = NULL;

  // Don't wait for pending registrations, if any, these hosts are buggy.
  DCHECK(registrar_.IsEmpty()) << "Some render process hosts where not "
                               << "destroyed early enough!";
  registrar_.RemoveAll();

  // And stop the timer so we can be released early too.
  timer_.Stop();
}

// static
bool ProfileDestroyer::GetHostsForProfile(
    Profile* const profile, std::vector<content::RenderProcessHost*>* hosts) {
  for (content::RenderProcessHost::iterator iter(
        content::RenderProcessHost::AllHostsIterator());
      !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    if (render_process_host && Profile::FromBrowserContext(
          render_process_host->GetBrowserContext()) == profile) {
      hosts->push_back(render_process_host);
    }
  }
  return !hosts->empty();
}
