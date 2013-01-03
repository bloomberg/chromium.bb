// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_dispatcher.h"

#include "base/logging.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/renderer/prerender/prerender_extra_data.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPrerenderingSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"

namespace prerender {

using WebKit::WebPrerender;
using WebKit::WebPrerenderingSupport;

PrerenderDispatcher::PrerenderDispatcher() {
  WebPrerenderingSupport::initialize(this);
}

PrerenderDispatcher::~PrerenderDispatcher() {
  WebPrerenderingSupport::shutdown();
}

bool PrerenderDispatcher::IsPrerenderURL(const GURL& url) const {
  return running_prerender_urls_.count(url) >= 1;
}

void PrerenderDispatcher::OnPrerenderStart(int prerender_id) {
  DCHECK_NE(0u, prerenders_.count(prerender_id));
  std::map<int, WebPrerender>::iterator it = prerenders_.find(prerender_id);

  WebPrerender& prerender = it->second;

  // The prerender should only be null in unit tests.
  if (prerender.isNull())
    return;

  prerender.didStartPrerender();
  OnPrerenderAddAlias(prerender_id, prerender.url());
}

void PrerenderDispatcher::OnPrerenderStopLoading(int prerender_id) {
  DCHECK_NE(0u, prerenders_.count(prerender_id));
  std::map<int, WebPrerender>::iterator it = prerenders_.find(prerender_id);

  WebPrerender& prerender = it->second;
  DCHECK(!prerender.isNull())
      << "OnPrerenderStopLoading shouldn't be called from a unit test, the only"
      << "context in which a WebPrerender in the dispatcher can be null.";

  prerender.didSendLoadForPrerender();
}

void PrerenderDispatcher::OnPrerenderAddAlias(int prerender_id,
                                              const GURL& url) {
  DCHECK_NE(0u, prerenders_.count(prerender_id));
  running_prerender_urls_.insert(
      std::multimap<GURL, int>::value_type(url, prerender_id));
}

void PrerenderDispatcher::OnPrerenderStop(int prerender_id) {
  DCHECK_NE(0u, prerenders_.count(prerender_id));
  WebPrerender& prerender = prerenders_[prerender_id];

  // The prerender should only be null in unit tests.
  if (!prerender.isNull())
    prerender.didStopPrerender();

  // TODO(cbentzel): We'd also want to send the map of active prerenders when
  // creating a new render process, so the Add/Remove go relative to that.
  // This may not be that big of a deal in practice, since the newly created tab
  // is unlikely to go to the prerendered page.
  prerenders_.erase(prerender_id);

  std::multimap<GURL, int>::iterator it = running_prerender_urls_.begin();
  while (it != running_prerender_urls_.end()) {
    std::multimap<GURL, int>::iterator next = it;
    ++next;

    if (it->second == prerender_id)
      running_prerender_urls_.erase(it);

    it = next;
  }
}

bool PrerenderDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderDispatcher, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderStart, OnPrerenderStart)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderStopLoading,
                        OnPrerenderStopLoading)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderAddAlias, OnPrerenderAddAlias)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderStop, OnPrerenderStop)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderDispatcher::add(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  prerenders_[extra_data.prerender_id()] = prerender;

  content::RenderThread::Get()->Send(new PrerenderHostMsg_AddLinkRelPrerender(
      extra_data.prerender_id(), GURL(prerender.url()),
      content::Referrer(GURL(prerender.referrer()),
                        prerender.referrerPolicy()),
      extra_data.size(), extra_data.render_view_route_id()));
}

void PrerenderDispatcher::cancel(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_CancelLinkRelPrerender(extra_data.prerender_id()));
}

void PrerenderDispatcher::abandon(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_AbandonLinkRelPrerender(extra_data.prerender_id()));
}

}  // namespace prerender
