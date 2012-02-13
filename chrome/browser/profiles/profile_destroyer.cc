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

// static
void ProfileDestroyer::DestroyOffTheRecordProfile(Profile* const profile) {
  std::vector<content::RenderProcessHost*> hosts;
  if (GetHostsForProfile(profile, &hosts)) {
    // The instance will destroy itself once all render process hosts referring
    // to it are properly terminated.
    scoped_refptr<ProfileDestroyer> profile_destroyer(
        new ProfileDestroyer(profile, hosts));
  } else {
    // Safe to destroy now... We're done...
    profile->GetOriginalProfile()->DestroyOffTheRecordProfile();
  }
}

ProfileDestroyer::ProfileDestroyer(
    Profile* const profile,
    const std::vector<content::RenderProcessHost*>& hosts) : profile_(profile) {
  for (size_t i = 0; i < hosts.size(); ++i) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::Source<content::RenderProcessHost>(hosts[i]));
    // For each of the notifications, we bump up our reference count.
    // It will go back to 0 and free us when all hosts are terminated.
    AddRef();
  }
}

ProfileDestroyer::~ProfileDestroyer() {
  // Check again, in case other render hosts were added while we were
  // waiting for the previous ones to go away...
  DestroyOffTheRecordProfile(profile_);
}

void ProfileDestroyer::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  // Delay the destruction one step further in case other observers of this
  // notification need to look at the profile attached to the host.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ProfileDestroyer::Release, this));
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
