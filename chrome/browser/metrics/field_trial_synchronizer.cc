// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/field_trial_synchronizer.h"

#include "base/logging.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

FieldTrialSynchronizer::FieldTrialSynchronizer() {
  DCHECK(field_trial_synchronizer_ == NULL);
  field_trial_synchronizer_ = this;
  base::FieldTrialList::AddObserver(this);
}

FieldTrialSynchronizer::~FieldTrialSynchronizer() {
  base::FieldTrialList::RemoveObserver(this);
  field_trial_synchronizer_ = NULL;
}

void FieldTrialSynchronizer::NotifyAllRenderers(
    const std::string& field_trial_name,
    const std::string& group_name) {
  // To iterate over RenderProcessHosts, or to send messages to the hosts, we
  // need to be on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (content::RenderProcessHost::iterator it(
          content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(
        new ChromeViewMsg_SetFieldTrialGroup(field_trial_name, group_name));
  }
}

void FieldTrialSynchronizer::OnFieldTrialGroupFinalized(
    const std::string& field_trial_name,
    const std::string& group_name) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &FieldTrialSynchronizer::NotifyAllRenderers,
                        field_trial_name,
                        group_name));
}

// static
FieldTrialSynchronizer*
    FieldTrialSynchronizer::field_trial_synchronizer_ = NULL;
