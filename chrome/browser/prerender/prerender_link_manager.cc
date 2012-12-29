// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager.h"

#include <limits>
#include <set>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

using base::TimeDelta;
using base::TimeTicks;
using content::RenderViewHost;
using content::SessionStorageNamespace;

namespace {

void Send(int child_id, IPC::Message* raw_message) {
  using content::RenderProcessHost;
  scoped_ptr<IPC::Message> own_message(raw_message);

  RenderProcessHost* render_process_host = RenderProcessHost::FromID(child_id);
  if (!render_process_host)
    return;
  render_process_host->Send(own_message.release());
}

}  // namespace

namespace prerender {

PrerenderLinkManager::PrerenderLinkManager(PrerenderManager* manager)
    : has_shutdown_(false),
      manager_(manager) {
}

PrerenderLinkManager::~PrerenderLinkManager() {
  for (std::list<LinkPrerender>::iterator i = prerenders_.begin();
       i != prerenders_.end(); ++i) {
    if (i->handle) {
      DCHECK(!i->handle->IsPrerendering())
          << "All running prerenders should stop at the same time as the "
          << "PrerenderManager.";
      delete i->handle;
      i->handle = 0;
    }
  }
}

void PrerenderLinkManager::OnAddPrerender(int launcher_child_id,
                                          int prerender_id,
                                          const GURL& url,
                                          const content::Referrer& referrer,
                                          const gfx::Size& size,
                                          int render_view_route_id) {
  DCHECK_EQ(static_cast<LinkPrerender*>(NULL),
            FindByLauncherChildIdAndPrerenderId(launcher_child_id,
                                                prerender_id));
  LinkPrerender
      prerender(launcher_child_id, prerender_id, url, referrer, size,
                render_view_route_id, manager_->GetCurrentTimeTicks());
  prerenders_.push_back(prerender);
  StartPrerenders();
}

void PrerenderLinkManager::OnCancelPrerender(int child_id, int prerender_id) {
  LinkPrerender* prerender = FindByLauncherChildIdAndPrerenderId(child_id,
                                                             prerender_id);
  if (!prerender)
    return;

  // Remove the handle from the PrerenderLinkManager before we cancel this
  // prerender, to avoid reentering the PrerenderLinkManager, sending events to
  // the underlying prerender and making a second erase.
  scoped_ptr<PrerenderHandle> own_prerender_handle(prerender->handle);
  prerender->handle = NULL;
  RemovePrerender(prerender);

  if (own_prerender_handle)
    own_prerender_handle->OnCancel();

  StartPrerenders();
}

void PrerenderLinkManager::OnAbandonPrerender(int child_id, int prerender_id) {
  LinkPrerender* prerender = FindByLauncherChildIdAndPrerenderId(child_id,
                                                             prerender_id);
  if (!prerender)
    return;

  if (!prerender->handle) {
    RemovePrerender(prerender);
    return;
  }

  prerender->handle->OnNavigateAway();
  DCHECK(prerender->handle);

  // If the prerender is not running, remove it from the list so it does not
  // leak. If it is running, it will send a cancel event when it stops which
  // will remove it.
  if (!prerender->handle->IsPrerendering())
    RemovePrerender(prerender);
}

void PrerenderLinkManager::OnChannelClosing(int child_id) {
  std::list<LinkPrerender>::iterator next = prerenders_.begin();
  while (next != prerenders_.end()) {
    std::list<LinkPrerender>::iterator it = next;
    ++next;

    if (child_id != it->launcher_child_id)
      continue;

    const size_t running_prerender_count = CountRunningPrerenders();
    OnAbandonPrerender(child_id, it->prerender_id);
    DCHECK_EQ(running_prerender_count, CountRunningPrerenders());
  }
}

PrerenderLinkManager::LinkPrerender::LinkPrerender(
    int launcher_child_id,
    int prerender_id,
    const GURL& url,
    const content::Referrer& referrer,
    const gfx::Size& size,
    int render_view_route_id,
    TimeTicks creation_time) : launcher_child_id(launcher_child_id),
                               prerender_id(prerender_id),
                               url(url),
                               referrer(referrer),
                               size(size),
                               render_view_route_id(render_view_route_id),
                               creation_time(creation_time),
                               handle(NULL) {
}

PrerenderLinkManager::LinkPrerender::~LinkPrerender() {
  DCHECK_EQ(static_cast<PrerenderHandle*>(NULL), handle)
      << "The PrerenderHandle should be destroyed before its Prerender.";
}

bool PrerenderLinkManager::IsEmpty() const {
  return prerenders_.empty();
}

size_t PrerenderLinkManager::CountRunningPrerenders() const {
  size_t retval = 0;
  for (std::list<LinkPrerender>::const_iterator i = prerenders_.begin();
       i != prerenders_.end(); ++i) {
    if (i->handle && i->handle->IsPrerendering())
      ++retval;
  }
  return retval;
}

void PrerenderLinkManager::StartPrerenders() {
  if (has_shutdown_)
    return;

  size_t total_started_prerender_count = 0;
  std::multiset<std::pair<int, int> >
      running_launcher_and_render_view_routes;

  // Scan the list, counting how many prerenders have handles (and so were added
  // to the PrerenderManager). The count is done for the system as a whole, and
  // also per launcher.
  for (std::list<LinkPrerender>::iterator i = prerenders_.begin();
       i != prerenders_.end(); ++i) {
    if (i->handle) {
      ++total_started_prerender_count;
      std::pair<int, int> launcher_and_render_view_route(
          i->launcher_child_id, i->render_view_route_id);
      running_launcher_and_render_view_routes.insert(
          launcher_and_render_view_route);
      DCHECK_GE(manager_->config().max_link_concurrency_per_launcher,
                running_launcher_and_render_view_routes.count(
                    launcher_and_render_view_route));
    }

    DCHECK_EQ(&(*i), FindByLauncherChildIdAndPrerenderId(i->launcher_child_id,
                                                         i->prerender_id));
  }
  DCHECK_GE(manager_->config().max_link_concurrency,
            total_started_prerender_count);
  DCHECK_LE(CountRunningPrerenders(), total_started_prerender_count);

  TimeTicks now = manager_->GetCurrentTimeTicks();

  // Scan the list again, starting prerenders as our counts allow.
  std::list<LinkPrerender>::iterator next = prerenders_.begin();
  while (next != prerenders_.end()) {
    std::list<LinkPrerender>::iterator i = next;
    ++next;

    if (total_started_prerender_count >=
            manager_->config().max_link_concurrency ||
        total_started_prerender_count >= prerenders_.size()) {
      // The system is already at its prerender concurrency limit.
      return;
    }

    if (i->handle) {
      // This prerender has already been added to the prerender manager.
      continue;
    }

    TimeDelta prerender_age = now - i->creation_time;
    if (prerender_age >= manager_->config().max_wait_to_launch) {
      // This prerender waited too long in the queue before launching.
      prerenders_.erase(i);
      continue;
    }

    std::pair<int, int> launcher_and_render_view_route(
        i->launcher_child_id, i->render_view_route_id);
    if (manager_->config().max_link_concurrency_per_launcher <=
        running_launcher_and_render_view_routes.count(
            launcher_and_render_view_route)) {
      // This prerender's launcher is already at its limit.
      continue;
    }

    PrerenderHandle* handle = manager_->AddPrerenderFromLinkRelPrerender(
        i->launcher_child_id, i->render_view_route_id,
        i->url, i->referrer, i->size);
    if (!handle) {
      // This prerender couldn't be launched, it's gone.
      prerenders_.erase(i);
      continue;
    }

    // We have successfully started a new prerender.
    i->handle = handle;
    ++total_started_prerender_count;
    handle->SetObserver(this);
    if (handle->IsPrerendering())
      OnPrerenderStart(handle);

    running_launcher_and_render_view_routes.insert(
        launcher_and_render_view_route);
  }
}

PrerenderLinkManager::LinkPrerender*
PrerenderLinkManager::FindByLauncherChildIdAndPrerenderId(int launcher_child_id,
                                                          int prerender_id) {
  for (std::list<LinkPrerender>::iterator i = prerenders_.begin();
       i != prerenders_.end(); ++i) {
    if (launcher_child_id == i->launcher_child_id &&
        prerender_id == i->prerender_id) {
      return &(*i);
    }
  }
  return NULL;
}

PrerenderLinkManager::LinkPrerender*
PrerenderLinkManager::FindByPrerenderHandle(PrerenderHandle* prerender_handle) {
  DCHECK(prerender_handle);
  for (std::list<LinkPrerender>::iterator i = prerenders_.begin();
       i != prerenders_.end(); ++i) {
    if (prerender_handle == i->handle)
      return &(*i);
  }
  return NULL;
}

void PrerenderLinkManager::RemovePrerender(LinkPrerender* prerender) {
  for (std::list<LinkPrerender>::iterator i = prerenders_.begin();
       i != prerenders_.end(); ++i) {
    if (&(*i) == prerender) {
      scoped_ptr<PrerenderHandle> own_handle(i->handle);
      i->handle = NULL;
      prerenders_.erase(i);
      return;
    }
  }
  NOTREACHED();
}

void PrerenderLinkManager::Shutdown() {
  has_shutdown_ = true;
}

// In practice, this is always called from either
// PrerenderLinkManager::OnAddPrerender in the regular case, or in the pending
// prerender case, from PrerenderHandle::AdoptPrerenderDataFrom.
void PrerenderLinkManager::OnPrerenderStart(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;
  Send(prerender->launcher_child_id,
       new PrerenderMsg_OnPrerenderStart(prerender->prerender_id));
}

void PrerenderLinkManager::OnPrerenderAddAlias(
    PrerenderHandle* prerender_handle,
    const GURL& alias_url) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;
  Send(prerender->launcher_child_id,
       new PrerenderMsg_OnPrerenderAddAlias(prerender->prerender_id,
                                            alias_url));
}

void PrerenderLinkManager::OnPrerenderStop(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  Send(prerender->launcher_child_id,
       new PrerenderMsg_OnPrerenderStop(prerender->prerender_id));
  RemovePrerender(prerender);
  StartPrerenders();
}

}  // namespace prerender
