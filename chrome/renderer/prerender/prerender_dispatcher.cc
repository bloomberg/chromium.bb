// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_dispatcher.h"

#include "base/logging.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/renderer/prerender/prerendering_support.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPrerenderingSupport.h"

namespace prerender {

PrerenderDispatcher::PrerenderDispatcher() {
  WebKit::WebPrerenderingSupport::initialize(new PrerenderingSupport());
}

PrerenderDispatcher::~PrerenderDispatcher() {
}

bool PrerenderDispatcher::IsPrerenderURL(const GURL& url) const {
  return prerender_urls_.find(url) != prerender_urls_.end();
}

bool PrerenderDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
 bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderDispatcher, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_AddPrerenderURL, OnAddPrerenderURL)
    IPC_MESSAGE_HANDLER(PrerenderMsg_RemovePrerenderURL, OnRemovePrerenderURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderDispatcher::OnAddPrerenderURL(const GURL& url) {
  PrerenderMap::iterator it = prerender_urls_.find(url);
  if (it != prerender_urls_.end()) {
    DCHECK(it->second > 0);
    it->second += 1;
  } else {
    prerender_urls_[url] = 1;
  }
}

void PrerenderDispatcher::OnRemovePrerenderURL(const GURL& url) {
  PrerenderMap::iterator it = prerender_urls_.find(url);
  // This is possible with a spurious remove.
  // TODO(cbentzel): We'd also want to send the map of active prerenders when
  // creating a new render process, so the Add/Remove go relative to that.
  // This may not be that big of a deal in practice, since the newly created tab
  // is unlikely to go to the prerendered page.
  if (it == prerender_urls_.end())
    return;
  DCHECK(it->second > 0);
  it->second -= 1;
  if (it->second == 0)
    prerender_urls_.erase(it);
}

}  // namespace prerender
