// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/browser/visitedlink_event_listener.h"

#include "base/bind.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/common/visitedlink.mojom.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "mojo/public/cpp/bindings/remote.h"

using base::Time;
using base::TimeDelta;
using content::RenderWidgetHost;

namespace {

// The amount of time we wait to accumulate visited link additions.
constexpr int kCommitIntervalMs = 100;

// Size of the buffer after which individual link updates deemed not warranted
// and the overall update should be used instead.
const unsigned kVisitedLinkBufferThreshold = 50;

}  // namespace

namespace visitedlink {

// This class manages buffering and sending visited link hashes (fingerprints)
// to renderer based on widget visibility.
// As opposed to the VisitedLinkEventListener, which coalesces to
// reduce the rate of messages being sent to render processes, this class
// ensures that the updates occur only when explicitly requested. This is
// used for RenderProcessHostImpl to only send Add/Reset link events to the
// renderers when their tabs are visible and the corresponding RenderViews are
// created.
class VisitedLinkUpdater {
 public:
  explicit VisitedLinkUpdater(int render_process_id)
      : reset_needed_(false),
        invalidate_hashes_(false),
        render_process_id_(render_process_id) {
    content::RenderProcessHost::FromID(render_process_id)
        ->BindReceiver(sink_.BindNewPipeAndPassReceiver());
  }

  // Informs the renderer about a new visited link table.
  void SendVisitedLinkTable(base::ReadOnlySharedMemoryRegion* region) {
    if (region->IsValid())
      sink_->UpdateVisitedLinks(region->Duplicate());
  }

  // Buffers |links| to update, but doesn't actually relay them.
  void AddLinks(const VisitedLinkCommon::Fingerprints& links) {
    if (reset_needed_)
      return;

    if (pending_.size() + links.size() > kVisitedLinkBufferThreshold) {
      // Once the threshold is reached, there's no need to store pending visited
      // link updates -- we opt for resetting the state for all links.
      AddReset(false);
      return;
    }

    pending_.insert(pending_.end(), links.begin(), links.end());
  }

  // Tells the updater that sending individual link updates is no longer
  // necessary and the visited state for all links should be reset. If
  // |invalidateHashes| is true all cached visited links hashes should be
  // dropped.
  void AddReset(bool invalidate_hashes) {
    reset_needed_ = true;
    // Do not set to false. If tab is invisible the reset message will not be
    // sent until tab became visible.
    if (invalidate_hashes)
      invalidate_hashes_ = true;
    pending_.clear();
  }

  // Sends visited link update messages: a list of links whose visited state
  // changed or reset of visited state for all links.
  void Update() {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(render_process_id_);
    if (!process)
      return;  // Happens in tests

    if (!process->VisibleClientCount())
      return;

    if (reset_needed_) {
      sink_->ResetVisitedLinks(invalidate_hashes_);
      reset_needed_ = false;
      invalidate_hashes_ = false;
      return;
    }

    if (pending_.empty())
      return;

    sink_->AddVisitedLinks(pending_);

    pending_.clear();
  }

 private:
  bool reset_needed_;
  bool invalidate_hashes_;
  int render_process_id_;
  mojo::Remote<mojom::VisitedLinkNotificationSink> sink_;
  VisitedLinkCommon::Fingerprints pending_;
};

VisitedLinkEventListener::VisitedLinkEventListener(
    content::BrowserContext* browser_context)
    : coalesce_timer_(&default_coalesce_timer_),
      browser_context_(browser_context) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

VisitedLinkEventListener::~VisitedLinkEventListener() {
  if (!pending_visited_links_.empty())
    pending_visited_links_.clear();
}

void VisitedLinkEventListener::NewTable(
    base::ReadOnlySharedMemoryRegion* table_region) {
  DCHECK(table_region && table_region->IsValid());
  table_region_ = table_region->Duplicate();
  if (!table_region_.IsValid())
    return;

  // Send to all RenderProcessHosts.
  for (auto i = updaters_.begin(); i != updaters_.end(); ++i) {
    // Make sure to not send to incognito renderers.
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(i->first);
    if (!process)
      continue;

    i->second->SendVisitedLinkTable(&table_region_);
  }
}

void VisitedLinkEventListener::Add(VisitedLinkWriter::Fingerprint fingerprint) {
  pending_visited_links_.push_back(fingerprint);

  if (!coalesce_timer_->IsRunning()) {
    coalesce_timer_->Start(
        FROM_HERE, TimeDelta::FromMilliseconds(kCommitIntervalMs),
        base::BindOnce(&VisitedLinkEventListener::CommitVisitedLinks,
                       base::Unretained(this)));
  }
}

void VisitedLinkEventListener::Reset(bool invalidate_hashes) {
  pending_visited_links_.clear();
  coalesce_timer_->Stop();

  for (auto i = updaters_.begin(); i != updaters_.end(); ++i) {
    i->second->AddReset(invalidate_hashes);
    i->second->Update();
  }
}

void VisitedLinkEventListener::SetCoalesceTimerForTest(
    base::OneShotTimer* coalesce_timer_override) {
  coalesce_timer_ = coalesce_timer_override;
}

void VisitedLinkEventListener::CommitVisitedLinks() {
  // Send to all RenderProcessHosts.
  for (auto i = updaters_.begin(); i != updaters_.end(); ++i) {
    i->second->AddLinks(pending_visited_links_);
    i->second->Update();
  }

  pending_visited_links_.clear();
}

void VisitedLinkEventListener::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      if (browser_context_ != process->GetBrowserContext())
        return;

      // Happens on browser start up.
      if (!table_region_.IsValid())
        return;

      updaters_[process->GetID()].reset(
          new VisitedLinkUpdater(process->GetID()));
      updaters_[process->GetID()]->SendVisitedLinkTable(&table_region_);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      if (updaters_.count(process->GetID())) {
        updaters_.erase(process->GetID());
      }
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED: {
      RenderWidgetHost* widget =
          content::Source<RenderWidgetHost>(source).ptr();
      int child_id = widget->GetProcess()->GetID();
      if (updaters_.count(child_id))
        updaters_[child_id]->Update();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace visitedlink
