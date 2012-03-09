// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "content/public/renderer/render_process_observer.h"

class GURL;

namespace prerender {

// PrerenderDispatcher keeps track of which URLs are being prerendered. There
// is only one PrerenderDispatcher per render process, and it will only be
// aware of prerenders that are triggered by this render process.
class PrerenderDispatcher : public content::RenderProcessObserver {
 public:
  PrerenderDispatcher();
  virtual ~PrerenderDispatcher();

  bool IsPrerenderURL(const GURL & url) const;

 private:
  void OnAddPrerenderURL(const GURL& url);
  void OnRemovePrerenderURL(const GURL& url);

  // RenderProcessObserver:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  typedef std::map<GURL, int> PrerenderMap;
  PrerenderMap prerender_urls_;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
