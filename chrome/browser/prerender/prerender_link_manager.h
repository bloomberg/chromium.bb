// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_

#include <list>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/prerender/prerender_handle.h"
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

class PrerenderHandle;
class PrerenderManager;

// PrerenderLinkManager implements the API on Link elements for all documents
// being rendered in this chrome instance.  It receives messages from the
// renderer indicating addition, cancelation and abandonment of link elements,
// and controls the PrerenderManager accordingly.
class PrerenderLinkManager : public ProfileKeyedService,
                             public PrerenderHandle::Observer {
 public:
  explicit PrerenderLinkManager(PrerenderManager* manager);
  virtual ~PrerenderLinkManager();

  // A <link rel=prerender ...> element has been inserted into the document.
  // The |prerender_id| must be unique per |child_id|, and is assigned by the
  // WebPrerendererClient.
  void OnAddPrerender(int child_id,
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

  struct LinkPrerender {
    LinkPrerender(int launcher_child_id,
                  int prerender_id,
                  const GURL& url,
                  const content::Referrer& referrer,
                  const gfx::Size& size,
                  int render_view_route_id,
                  base::TimeTicks creation_time);
    ~LinkPrerender();

    // Parameters from PrerenderLinkManager::OnAddPrerender():
    int launcher_child_id;
    int prerender_id;
    GURL url;
    content::Referrer referrer;
    gfx::Size size;
    int render_view_route_id;

    // The time at which this Prerender was added to PrerenderLinkManager.
    base::TimeTicks creation_time;

    // Initially NULL, |handle| is set once we start this prerender. It is owned
    // by this struct, and must be deleted before destructing this struct.
    PrerenderHandle* handle;
  };

  bool IsEmpty() const;

  // Returns a count of currently running prerenders.
  size_t CountRunningPrerenders() const;

  // Start any prerenders that can be started, respecting concurrency limits for
  // the system and per launcher.
  void StartPrerenders();

  LinkPrerender* FindByLauncherChildIdAndPrerenderId(int child_id,
                                                     int prerender_id);

  LinkPrerender* FindByPrerenderHandle(PrerenderHandle* prerender_handle);

  void RemovePrerender(LinkPrerender* prerender);

  // From ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // From PrerenderHandle::Observer:
  virtual void OnPrerenderStart(PrerenderHandle* prerender_handle) OVERRIDE;
  virtual void OnPrerenderStopLoading(PrerenderHandle* prerender_handle)
      OVERRIDE;
  virtual void OnPrerenderStop(PrerenderHandle* prerender_handle) OVERRIDE;
  virtual void OnPrerenderAddAlias(PrerenderHandle* prerender_handle,
                                   const GURL& alias_url) OVERRIDE;

  bool has_shutdown_;

  PrerenderManager* manager_;

  // All prerenders known to this PrerenderLinkManager. Insertions are always
  // made at the back, so the oldest prerender is at the front, and the youngest
  // at the back.
  std::list<LinkPrerender> prerenders_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderLinkManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_

