// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_
#pragma once

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace content {
struct Referrer;
}

namespace gfx {
class Size;
}

namespace prerender {

class PrerenderManager;

// PrerenderLinkManager implements the API on Link elements for all documents
// being rendered in this chrome instance.  It receives messages from the
// renderer indicating addition, cancelation and abandonment of link elements,
// and controls the PrerenderManager accordingly.
class PrerenderLinkManager : public ProfileKeyedService {
 public:
  explicit PrerenderLinkManager(PrerenderManager* manager);
  virtual ~PrerenderLinkManager();

  // A <link rel=prerender ...> element has been inserted into the document.
  // The |prerender_id| must be unique per |child_id|, and is assigned by the
  // WebPrerendererClient.
  // Returns true if the prerender was accepted by the prerender manager,
  // and false if not.  In either case, the |prerender_id| is usable for
  // future OnCancelPrerender and OnAbandonPrerender calls.
  bool OnAddPrerender(
      int child_id,
      int prerender_id,
      const GURL& url,
      const content::Referrer& referrer,
      const gfx::Size& size,
      int render_view_route_id);

  // A <link rel=prerender ...> element has been explicitly removed from a
  // document.
  void OnCancelPrerender(int child_id, int prerender_id);

  // A renderer launching <link rel=prerender ...> has navigated away from the
  // launching page, the launching renderer process has crashed, or perhaps the
  // renderer process was fast-closed when the last render view in it was
  // closed.
  void OnAbandonPrerender(int child_id, int prerender_id);

  // If a renderer channel closes (crash, fast exit, etc...), that's effectively
  // an abandon of any prerenders launched by that child.
  void OnChannelClosing(int child_id);

 private:
  friend class PrerenderBrowserTest;
  friend class PrerenderTest;

  typedef std::pair<int, int> ChildAndPrerenderIdPair;
  typedef std::map<ChildAndPrerenderIdPair, GURL> IdPairToUrlMap;

  void RemovePrerender(const IdPairToUrlMap::iterator& id_url_iter);

  bool IsEmpty() const;

  PrerenderManager* manager_;
  IdPairToUrlMap ids_to_url_map_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderLinkManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_

