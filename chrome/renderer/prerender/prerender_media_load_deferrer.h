// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_MEDIA_LOAD_DEFERRER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_MEDIA_LOAD_DEFERRER_H_

#include "base/callback.h"
#include "content/public/renderer/render_view_observer.h"

namespace prerender {

// Defers media player loading in prerendered pages until the prerendered page
// is swapped in.
class PrerenderMediaLoadDeferrer : public content::RenderViewObserver {
 public:
  // Will run |closure| to continue loading the media resource once the page is
  // swapped in.
  PrerenderMediaLoadDeferrer(content::RenderView* render_view,
                             const base::Closure& closure);
  virtual ~PrerenderMediaLoadDeferrer();

 private:
  // RenderViewObserver method:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSetIsPrerendering(bool is_prerendering);

  bool is_prerendering_;
  base::Closure continue_loading_cb_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderMediaLoadDeferrer);
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_MEDIA_LOAD_DEFERRER_H_
