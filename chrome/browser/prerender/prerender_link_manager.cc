// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager.h"

#include <limits>
#include <queue>
#include <utility>

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

using content::RenderViewHost;
using content::SessionStorageNamespace;

namespace prerender {

PrerenderLinkManager::PrerenderLinkManager(PrerenderManager* manager)
    : manager_(manager) {
}

PrerenderLinkManager::~PrerenderLinkManager() {
  for (IdPairToPrerenderHandleMap::iterator it = ids_to_handle_map_.begin();
       it != ids_to_handle_map_.end();
       ++it) {
    PrerenderHandle* prerender_handle = it->second;
    prerender_handle->OnCancel();
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

  const ChildAndPrerenderIdPair child_and_prerender_id(child_id, prerender_id);
  DCHECK_EQ(0U, ids_to_handle_map_.count(child_and_prerender_id));

  scoped_ptr<PrerenderHandle> prerender_handle(
      manager_->AddPrerenderFromLinkRelPrerender(
          child_id, render_view_route_id, url, referrer, size));
  if (prerender_handle.get()) {
    std::pair<IdPairToPrerenderHandleMap::iterator, bool> insert_result =
        ids_to_handle_map_.insert(std::make_pair(
            child_and_prerender_id, static_cast<PrerenderHandle*>(NULL)));
    DCHECK(insert_result.second);
    delete insert_result.first->second;
    insert_result.first->second = prerender_handle.release();
    return true;
  }
  return false;
}

// TODO(gavinp): Once an observer interface is provided down to the WebKit
// layer, we should add DCHECK_NE(0L, ids_to_url_map_.count(...)) to both
// OnCancelPrerender and OnAbandonPrerender. We can't do this now, since
// the WebKit layer isn't even aware if we didn't add the prerender to the map
// in OnAddPrerender above.
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
  PrerenderHandle* prerender_handle = id_to_handle_iter->second;
  prerender_handle->OnCancel();
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
  RemovePrerender(id_to_handle_iter);
}

void PrerenderLinkManager::OnChannelClosing(int child_id) {
  DVLOG(2) << "OnChannelClosing, child id = " << child_id;
  const ChildAndPrerenderIdPair child_and_minimum_prerender_id(
      child_id, std::numeric_limits<int>::min());
  const ChildAndPrerenderIdPair child_and_maximum_prerender_id(
      child_id, std::numeric_limits<int>::max());
  std::queue<int> prerender_ids_to_abandon;
  for (IdPairToPrerenderHandleMap::iterator
           i = ids_to_handle_map_.lower_bound(child_and_minimum_prerender_id),
           e = ids_to_handle_map_.upper_bound(child_and_maximum_prerender_id);
       i != e; ++i) {
    prerender_ids_to_abandon.push(i->first.second);
  }
  while (!prerender_ids_to_abandon.empty()) {
    DVLOG(4) << "---> abandon prerender_id = "
             << prerender_ids_to_abandon.front();
    OnAbandonPrerender(child_id, prerender_ids_to_abandon.front());
    prerender_ids_to_abandon.pop();
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

}  // namespace prerender
