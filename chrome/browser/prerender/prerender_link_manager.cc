// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager.h"

#include <limits>
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
    : manager_(manager) {
}

PrerenderLinkManager::~PrerenderLinkManager() {
  for (IdPairToPrerenderHandleMap::iterator it = ids_to_handle_map_.begin();
       it != ids_to_handle_map_.end();
       ++it) {
    PrerenderHandle* prerender_handle = it->second;
    DCHECK(!prerender_handle->IsPrerendering())
        << "All running prerenders should stop at the same time as the "
        << "PrerenderManager.";
    delete prerender_handle;
  }
}

bool PrerenderLinkManager::OnAddPrerender(int child_id,
                                          int prerender_id,
                                          const GURL& url,
                                          const content::Referrer& referrer,
                                          const gfx::Size& size,
                                          int render_view_route_id) {
  DVLOG(2) << "OnAddPrerender, child_id = " << child_id
           << ", prerender_id = " << prerender_id
           << ", url = " << url.spec();
  DVLOG(3) << "... referrer url = " << referrer.url.spec()
           << ", size = (" << size.width() << ", " << size.height() << ")"
           << ", render_view_route_id = " << render_view_route_id;


  PrerenderHandle* prerender_handle =
      manager_->AddPrerenderFromLinkRelPrerender(
          child_id, render_view_route_id, url, referrer, size);
  if (!prerender_handle)
    return false;

  const ChildAndPrerenderIdPair child_and_prerender_id(child_id, prerender_id);
  DCHECK_EQ(0u, ids_to_handle_map_.count(child_and_prerender_id));
  ids_to_handle_map_[child_and_prerender_id] = prerender_handle;

  // If we are given a prerender that is already prerendering, we have missed
  // the start event.
  if (prerender_handle->IsPrerendering())
    OnPrerenderStart(prerender_handle);
  prerender_handle->SetObserver(this);
  return true;
}

void PrerenderLinkManager::OnCancelPrerender(int child_id, int prerender_id) {
  DVLOG(2) << "OnCancelPrerender, child_id = " << child_id
           << ", prerender_id = " << prerender_id;
  const ChildAndPrerenderIdPair child_and_prerender_id(child_id, prerender_id);
  IdPairToPrerenderHandleMap::iterator id_to_handle_iter =
      ids_to_handle_map_.find(child_and_prerender_id);
  if (id_to_handle_iter == ids_to_handle_map_.end()) {
    DVLOG(5) << "... canceling a prerender that doesn't exist.";
    return;
  }

  scoped_ptr<PrerenderHandle> prerender_handle(id_to_handle_iter->second);
  ids_to_handle_map_.erase(id_to_handle_iter);
  prerender_handle->OnCancel();

  // Because OnCancel() can remove the prerender from the map, we need to
  // consider our iterator invalid.
  id_to_handle_iter = ids_to_handle_map_.find(child_and_prerender_id);
  if (id_to_handle_iter != ids_to_handle_map_.end())
    RemovePrerender(id_to_handle_iter);
}

void PrerenderLinkManager::OnAbandonPrerender(int child_id, int prerender_id) {
  DVLOG(2) << "OnAbandonPrerender, child_id = " << child_id
           << ", prerender_id = " << prerender_id;
  const ChildAndPrerenderIdPair child_and_prerender_id(child_id, prerender_id);
  IdPairToPrerenderHandleMap::iterator id_to_handle_iter =
      ids_to_handle_map_.find(child_and_prerender_id);
  if (id_to_handle_iter == ids_to_handle_map_.end())
    return;
  PrerenderHandle* prerender_handle = id_to_handle_iter->second;
  prerender_handle->OnNavigateAway();
}

void PrerenderLinkManager::OnChannelClosing(int child_id) {
  DVLOG(2) << "OnChannelClosing, child id = " << child_id;
  const ChildAndPrerenderIdPair child_and_minimum_prerender_id(
      child_id, std::numeric_limits<int>::min());
  const ChildAndPrerenderIdPair child_and_maximum_prerender_id(
      child_id, std::numeric_limits<int>::max());

  IdPairToPrerenderHandleMap::iterator
      it = ids_to_handle_map_.lower_bound(child_and_minimum_prerender_id);
  IdPairToPrerenderHandleMap::iterator
      end = ids_to_handle_map_.upper_bound(child_and_maximum_prerender_id);
  while (it != end) {
    IdPairToPrerenderHandleMap::iterator next = it;
    ++next;

    size_t size_before_abandon = ids_to_handle_map_.size();
    OnAbandonPrerender(child_id, it->first.second);
    DCHECK_EQ(size_before_abandon, ids_to_handle_map_.size());
    RemovePrerender(it);

    it = next;
  }
}

bool PrerenderLinkManager::IsEmpty() const {
  return ids_to_handle_map_.empty();
}

void PrerenderLinkManager::RemovePrerender(
    const IdPairToPrerenderHandleMap::iterator& id_to_handle_iter) {
  PrerenderHandle* prerender_handle = id_to_handle_iter->second;
  delete prerender_handle;
  ids_to_handle_map_.erase(id_to_handle_iter);
}

PrerenderLinkManager::IdPairToPrerenderHandleMap::iterator
PrerenderLinkManager::FindPrerenderHandle(
    PrerenderHandle* prerender_handle) {
  for (IdPairToPrerenderHandleMap::iterator it = ids_to_handle_map_.begin();
       it != ids_to_handle_map_.end(); ++it) {
    if (it->second == prerender_handle)
      return it;
  }
  return ids_to_handle_map_.end();
}

// In practice, this is always called from either
// PrerenderLinkManager::OnAddPrerender in the regular case, or in the pending
// prerender case, from PrerenderHandle::AdoptPrerenderDataFrom.
void PrerenderLinkManager::OnPrerenderStart(
    PrerenderHandle* prerender_handle) {
  IdPairToPrerenderHandleMap::iterator it =
      FindPrerenderHandle(prerender_handle);
  DCHECK(it != ids_to_handle_map_.end());
  const int child_id = it->first.first;
  const int prerender_id = it->first.second;

  Send(child_id, new PrerenderMsg_OnPrerenderStart(prerender_id));
}

void PrerenderLinkManager::OnPrerenderAddAlias(
    PrerenderHandle* prerender_handle,
    const GURL& alias_url) {
  IdPairToPrerenderHandleMap::iterator it =
      FindPrerenderHandle(prerender_handle);
  if (it == ids_to_handle_map_.end())
    return;
  const int child_id = it->first.first;
  const int prerender_id = it->first.second;

  Send(child_id, new PrerenderMsg_OnPrerenderAddAlias(prerender_id, alias_url));
}

void PrerenderLinkManager::OnPrerenderStop(
    PrerenderHandle* prerender_handle) {
  IdPairToPrerenderHandleMap::iterator it =
      FindPrerenderHandle(prerender_handle);
  if (it == ids_to_handle_map_.end())
    return;
  const int child_id = it->first.first;
  const int prerender_id = it->first.second;

  Send(child_id, new PrerenderMsg_OnPrerenderStop(prerender_id));
  RemovePrerender(it);
}

}  // namespace prerender
