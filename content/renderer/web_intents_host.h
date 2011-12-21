// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_INTENTS_HOST_H_
#define CONTENT_RENDERER_WEB_INTENTS_HOST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class RenderViewImpl;

namespace WebKit {
class WebFrame;
}

namespace webkit_glue {
struct WebIntentData;
}

// WebIntentsHost is a delegate for Web Intents messages. It is the
// renderer-side handler for IPC messages delivering the intent payload data
// and preparing it for access by the service page.
class WebIntentsHost : public content::RenderViewObserver {
 public:
  // |render_view| must not be NULL.
  explicit WebIntentsHost(RenderViewImpl* render_view);
  virtual ~WebIntentsHost();

  // Called by the bound intent object to register the result from the service
  // page.
  void OnResult(const WebKit::WebString& data);
  void OnFailure(const WebKit::WebString& data);

 private:
  class BoundDeliveredIntent;

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;

  // TODO(gbillock): Do we need various ***ClientRedirect methods to implement
  // intent cancelling policy? Figure this out.

  // On the service page, handler method for the IntentsMsg_SetWebIntent
  // message.
  void OnSetIntent(const webkit_glue::WebIntentData& intent);

  // On the client page, handler method for the IntentsMsg_WebIntentReply
  // message.
  void OnWebIntentReply(webkit_glue::WebIntentReplyType reply_type,
                        const WebKit::WebString& data,
                        int intent_id);

  // Delivered intent data from the caller.
  scoped_ptr<webkit_glue::WebIntentData> intent_;

  // Representation of the intent data as a C++ bound NPAPI object to be
  // injected into the Javascript context.
  scoped_ptr<BoundDeliveredIntent> delivered_intent_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsHost);
};

#endif  // CONTENT_RENDERER_WEB_INTENTS_HOST_H_
