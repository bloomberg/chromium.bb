// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPrerenderingSupport.h"

class GURL;

namespace prerender {

// There is one PrerenderDispatcher per render process. It keeps track of which
// prerenders were launched from this renderer, and ensures prerender navigation
// is triggered on navigation to those. It implements the prerendering interface
// supplied to WebKit.
class PrerenderDispatcher : public content::RenderProcessObserver,
                            public WebKit::WebPrerenderingSupport {
 public:
  PrerenderDispatcher();
  virtual ~PrerenderDispatcher();

  bool IsPrerenderURL(const GURL& url) const;

 private:
  friend class PrerenderDispatcherTest;

  void OnAddPrerenderURL(const GURL& url);
  void OnRemovePrerenderURL(const GURL& url);

  // From RenderProcessObserver:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // From WebPrerenderingSupport:
  virtual void add(const WebKit::WebPrerender& prerender) OVERRIDE;
  virtual void cancel(const WebKit::WebPrerender& prerender) OVERRIDE;
  virtual void abandon(const WebKit::WebPrerender& prerender) OVERRIDE;

  typedef std::map<GURL, int> PrerenderMap;
  PrerenderMap prerender_urls_;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
