// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"

class GURL;

namespace prerender {

class PrerenderingSupport;

// PrerenderDispatcher keeps track of which URLs are being prerendered. There
// is only one PrerenderDispatcher per render process, and it will only be
// aware of prerenders that are triggered by this render process.  As well,
// it holds on to other objects that must exist once per-renderer process,
// such as the PrerenderingSupport.
class PrerenderDispatcher : public content::RenderProcessObserver {
 public:
  PrerenderDispatcher();
  virtual ~PrerenderDispatcher();

  bool IsPrerenderURL(const GURL & url) const;

 private:
  friend class PrerenderDispatcherTest;

  void OnAddPrerenderURL(const GURL& url);
  void OnRemovePrerenderURL(const GURL& url);

  // RenderProcessObserver:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  typedef std::map<GURL, int> PrerenderMap;
  PrerenderMap prerender_urls_;

  // There is one PrerenderingSupport object per renderer, and it provides
  // the interface to prerendering to the WebKit platform.
  scoped_ptr<PrerenderingSupport> prerendering_support_;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
