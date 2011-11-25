// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_VIEW_HOST_OBSERVER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_VIEW_HOST_OBSERVER_H_

#include <vector>

#include "content/public/browser/render_view_host_observer.h"

struct FaviconURL;
class RenderViewHost;

namespace IPC {
class Message;
}

namespace prerender {

class PrerenderContents;

// Observer for RenderViewHost messages.
class PrerenderRenderViewHostObserver : public content::RenderViewHostObserver {
 public:
  PrerenderRenderViewHostObserver(PrerenderContents* prerender_contents,
                                  RenderViewHost* render_view_host);

  virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE;

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual bool Send(IPC::Message* message) OVERRIDE;

  void set_prerender_contents(PrerenderContents* prerender_contents) {
    prerender_contents_ = prerender_contents;
  }

 private:
  // Message handlers.
  void OnUpdateFaviconURL(int32 page_id, const std::vector<FaviconURL>& urls);
  void OnMaybeCancelPrerenderForHTML5Media();
  void OnCancelPrerenderForPrinting();

  // The associated prerender contents.
  PrerenderContents* prerender_contents_;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_VIEW_HOST_OBSERVER_H_
