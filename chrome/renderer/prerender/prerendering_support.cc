// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerendering_support.h"

#include "chrome/common/prerender_messages.h"
#include "chrome/renderer/prerender/prerender_extra_data.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPrerenderingSupport.h"

namespace prerender {

PrerenderingSupport::~PrerenderingSupport() {
}

void PrerenderingSupport::add(const WebKit::WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(new PrerenderHostMsg_AddLinkRelPrerender(
      extra_data.prerender_id(), GURL(prerender.url()),
      content::Referrer(GURL(prerender.referrer()), prerender.referrerPolicy()),
      extra_data.size(), extra_data.render_view_route_id()));
}

void PrerenderingSupport::cancel(const WebKit::WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_CancelLinkRelPrerender(extra_data.prerender_id()));
}

void PrerenderingSupport::abandon(const WebKit::WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_AbandonLinkRelPrerender(extra_data.prerender_id()));
}

}  // namespace prerender

