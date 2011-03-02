// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visitedlink/visitedlink_event_listener.h"

#include "base/shared_memory.h"
#include "content/browser/renderer_host/render_process_host.h"

using base::Time;
using base::TimeDelta;

// The amount of time we wait to accumulate visited link additions.
static const int kCommitIntervalMs = 100;

VisitedLinkEventListener::VisitedLinkEventListener() {
}

VisitedLinkEventListener::~VisitedLinkEventListener() {
}

void VisitedLinkEventListener::NewTable(base::SharedMemory* table_memory) {
  if (!table_memory)
    return;

  // Send to all RenderProcessHosts.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    if (!i.GetCurrentValue()->HasConnection())
      continue;

    i.GetCurrentValue()->SendVisitedLinkTable(table_memory);
  }
}

void VisitedLinkEventListener::Add(VisitedLinkMaster::Fingerprint fingerprint) {
  pending_visited_links_.push_back(fingerprint);

  if (!coalesce_timer_.IsRunning()) {
    coalesce_timer_.Start(
        TimeDelta::FromMilliseconds(kCommitIntervalMs), this,
        &VisitedLinkEventListener::CommitVisitedLinks);
  }
}

void VisitedLinkEventListener::Reset() {
  pending_visited_links_.clear();
  coalesce_timer_.Stop();

  // Send to all RenderProcessHosts.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->ResetVisitedLinks();
  }
}

void VisitedLinkEventListener::CommitVisitedLinks() {
  // Send to all RenderProcessHosts.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->AddVisitedLinks(pending_visited_links_);
  }

  pending_visited_links_.clear();
}
