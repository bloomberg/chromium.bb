// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager.h"

#include <limits>
#include <queue>
#include <utility>

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "ui/gfx/size.h"

using content::RenderViewHost;
using content::SessionStorageNamespace;

namespace prerender {

PrerenderLinkManager::PrerenderLinkManager(PrerenderManager* manager)
    : manager_(manager) {
}

PrerenderLinkManager::~PrerenderLinkManager() {
}

bool PrerenderLinkManager::OnAddPrerender(int child_id,
                                          int prerender_id,
                                          const GURL& orig_url,
                                          const content::Referrer& referrer,
                                          const gfx::Size& size,
                                          int render_view_route_id) {
  DVLOG(2) << "OnAddPrerender, child_id = " << child_id
           << ", prerender_id = " << prerender_id
           << ", url = " << orig_url.spec();
  DVLOG(3) << "... referrer url = " << referrer.url.spec()
           << ", size = (" << size.width() << ", " << size.height() << ")"
           << ", render_view_route_id = " << render_view_route_id;

  // TODO(gavinp): Add tests to ensure fragments work, then remove this fragment
  // clearing code.
  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  const GURL url = orig_url.ReplaceComponents(replacements);

  if (!manager_->AddPrerenderFromLinkRelPrerender(
          child_id, render_view_route_id, url, referrer, size)) {
    return false;
  }
  const ChildAndPrerenderIdPair child_and_prerender_id(child_id, prerender_id);
  DCHECK_EQ(0U, ids_to_url_map_.count(child_and_prerender_id));
  ids_to_url_map_.insert(std::make_pair(child_and_prerender_id, url));
  return true;
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
  IdPairToUrlMap::iterator id_url_iter =
      ids_to_url_map_.find(child_and_prerender_id);
  if (id_url_iter == ids_to_url_map_.end()) {
    DVLOG(5) << "... canceling a prerender that doesn't exist.";
    return;
  }
  const GURL url = id_url_iter->second;
  ids_to_url_map_.erase(id_url_iter);
  manager_->MaybeCancelPrerender(url);
}

void PrerenderLinkManager::OnAbandonPrerender(int child_id, int prerender_id) {
  DVLOG(2) << "OnAbandonPrerender, child_id = " << child_id
           << ", prerender_id = " << prerender_id;
  // TODO(gavinp,cbentzel): Implement reasonable behaviour for
  // navigation away from launcher.
  const ChildAndPrerenderIdPair child_and_prerender_id(child_id, prerender_id);
  ids_to_url_map_.erase(child_and_prerender_id);
}

void PrerenderLinkManager::OnChannelClosing(int child_id) {
  DVLOG(2) << "OnChannelClosing, child id = " << child_id;
  const ChildAndPrerenderIdPair child_and_minimum_prerender_id(
      child_id, std::numeric_limits<int>::min());
  const ChildAndPrerenderIdPair child_and_maximum_prerender_id(
      child_id, std::numeric_limits<int>::max());
  std::queue<int> prerender_ids_to_abandon;
  for (IdPairToUrlMap::iterator
           i = ids_to_url_map_.lower_bound(child_and_minimum_prerender_id),
           e = ids_to_url_map_.upper_bound(child_and_maximum_prerender_id);
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
  return ids_to_url_map_.empty();
}

}  // namespace prerender

