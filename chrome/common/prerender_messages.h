// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "content/public/common/common_param_traits.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START PrerenderMsgStart

// PrerenderLinkManager Messages
// These are messages sent from the renderer to the browser in
// relation to <link rel=prerender> elements.

IPC_STRUCT_BEGIN(PrerenderAttributes)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(uint32, rel_types)
IPC_STRUCT_END()

// Notifies of the insertion of a <link rel=prerender> element in the
// document.
IPC_MESSAGE_CONTROL5(PrerenderHostMsg_AddLinkRelPrerender,
                     int /* prerender_id, assigned by WebPrerendererClient */,
                     PrerenderAttributes,
                     content::Referrer,
                     gfx::Size,
                     int /* render_view_route_id of launcher */)

// Notifies on removal of a <link rel=prerender> element from the document.
IPC_MESSAGE_CONTROL1(PrerenderHostMsg_CancelLinkRelPrerender,
                     int /* prerender_id, assigned by WebPrerendererClient */)

// Notifies on unloading a <link rel=prerender> element from a frame.
IPC_MESSAGE_CONTROL1(PrerenderHostMsg_AbandonLinkRelPrerender,
                     int /* prerender_id, assigned by WebPrerendererClient */)

// PrerenderDispatcher Messages
// These are messages sent from the browser to the renderer in relation to
// running prerenders.

// Tells a renderer if it's currently being prerendered.  Must only be set
// to true before any navigation occurs, and only set to false at most once
// after that.
IPC_MESSAGE_ROUTED1(PrerenderMsg_SetIsPrerendering,
                    bool /* whether the RenderView is prerendering */)

// Signals to launcher that a prerender is running.
IPC_MESSAGE_CONTROL1(PrerenderMsg_OnPrerenderStart,
                     int /* prerender_id */)

// Signals to launcher that a prerender is running.
IPC_MESSAGE_CONTROL1(PrerenderMsg_OnPrerenderStopLoading,
                     int /* prerender_id */)

// Signals to launcher that a prerender has had it's 'domcontentloaded' event.
IPC_MESSAGE_CONTROL1(PrerenderMsg_OnPrerenderDomContentLoaded,
                     int /* prerender_id */)

// Signals to a launcher that a new alias has been added to a prerender.
IPC_MESSAGE_CONTROL1(PrerenderMsg_OnPrerenderAddAlias,
                     GURL /* url */)

// Signals to a launcher that a new alias has been added to a prerender.
IPC_MESSAGE_CONTROL1(PrerenderMsg_OnPrerenderRemoveAliases,
                     std::vector<GURL> /* urls */)

// Signals to a launcher that a prerender is no longer running.
IPC_MESSAGE_CONTROL1(PrerenderMsg_OnPrerenderStop,
                     int /* prerender_id */)
