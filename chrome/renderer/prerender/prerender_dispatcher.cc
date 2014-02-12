// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_dispatcher.h"

#include "base/logging.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/prerender_types.h"
#include "chrome/renderer/prerender/prerender_extra_data.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebPrerenderingSupport.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"

namespace prerender {

using blink::WebPrerender;
using blink::WebPrerenderingSupport;

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
  std::map<int, WebPrerender>::iterator it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;

  WebPrerender& prerender = it->second;

  // The prerender should only be null in unit tests.
  if (prerender.isNull())
    return;

  prerender.didStartPrerender();
}

void PrerenderDispatcher::OnPrerenderStopLoading(int prerender_id) {
  std::map<int, WebPrerender>::iterator it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;

  WebPrerender& prerender = it->second;
  DCHECK(!prerender.isNull())
      << "OnPrerenderStopLoading shouldn't be called from a unit test, the only"
      << "context in which a WebPrerender in the dispatcher can be null.";

  prerender.didSendLoadForPrerender();
}

void PrerenderDispatcher::OnPrerenderDomContentLoaded(int prerender_id) {
  std::map<int, WebPrerender>::iterator it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;

  WebPrerender& prerender = it->second;
  DCHECK(!prerender.isNull())
      << "OnPrerenderDomContentLoaded shouldn't be called from a unit test,"
      << " the only context in which a WebPrerender in the dispatcher can be"
      << " null.";

  prerender.didSendDOMContentLoadedForPrerender();
}

void PrerenderDispatcher::OnPrerenderAddAlias(const GURL& alias) {
  running_prerender_urls_.insert(alias);
}

void PrerenderDispatcher::OnPrerenderRemoveAliases(
    const std::vector<GURL>& aliases) {
  for (size_t i = 0; i < aliases.size(); ++i) {
    std::multiset<GURL>::iterator it = running_prerender_urls_.find(aliases[i]);
    if (it != running_prerender_urls_.end()) {
      running_prerender_urls_.erase(it);
    }
  }
}

void PrerenderDispatcher::OnPrerenderStop(int prerender_id) {
  std::map<int, WebPrerender>::iterator it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;
  WebPrerender& prerender = it->second;

  // The prerender should only be null in unit tests.
  if (!prerender.isNull())
    prerender.didStopPrerender();

  // TODO(cbentzel): We'd also want to send the map of active prerenders when
  // creating a new render process, so the Add/Remove go relative to that.
  // This may not be that big of a deal in practice, since the newly created tab
  // is unlikely to go to the prerendered page.
  prerenders_.erase(prerender_id);
}

bool PrerenderDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderDispatcher, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderStart, OnPrerenderStart)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderStopLoading,
                        OnPrerenderStopLoading)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderDomContentLoaded,
                        OnPrerenderDomContentLoaded)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderAddAlias, OnPrerenderAddAlias)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderRemoveAliases,
                        OnPrerenderRemoveAliases)
    IPC_MESSAGE_HANDLER(PrerenderMsg_OnPrerenderStop, OnPrerenderStop)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderDispatcher::add(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  if (prerenders_.count(extra_data.prerender_id()) != 0) {
    // TODO(gavinp): Determine why these apparently duplicate adds occur.
    return;
  }

  prerenders_[extra_data.prerender_id()] = prerender;

  PrerenderAttributes attributes;
  attributes.url = GURL(prerender.url());
  attributes.rel_types = prerender.relTypes();

  content::RenderThread::Get()->Send(new PrerenderHostMsg_AddLinkRelPrerender(
      extra_data.prerender_id(), attributes,
      content::Referrer(GURL(prerender.referrer()),
                        prerender.referrerPolicy()),
      extra_data.size(), extra_data.render_view_route_id()));
}

void PrerenderDispatcher::cancel(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_CancelLinkRelPrerender(extra_data.prerender_id()));
  // The browser will not send an OnPrerenderStop (the prerender may have even
  // been canceled before it was started), so release it to avoid a
  // leak. Moreover, if it did, the PrerenderClient in Blink will have been
  // detached already.
   prerenders_.erase(extra_data.prerender_id());
}

void PrerenderDispatcher::abandon(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_AbandonLinkRelPrerender(extra_data.prerender_id()));
  // The browser will not send an OnPrerenderStop (the prerender may have even
  // been canceled before it was started), so release it to avoid a
  // leak. Moreover, if it did, the PrerenderClient in Blink will have been
  // detached already.
  prerenders_.erase(extra_data.prerender_id());
}

}  // namespace prerender
