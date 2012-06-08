// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_INTENTS_HOST_H_
#define CONTENT_RENDERER_WEB_INTENTS_HOST_H_
#pragma once

#include <map>

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlob.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class RenderViewImpl;

namespace WebKit {
class WebDeliveredIntentClient;
class WebIntentRequest;
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

  // Called by the RenderView to register a new Web Intent invocation.
  int RegisterWebIntent(const WebKit::WebIntentRequest& request);

  // Called into when the delivered intent object gets a postResult call.
  void OnResult(const WebKit::WebString& data);
  // Called into when the delivered intent object gets a postFailure call.
  void OnFailure(const WebKit::WebString& data);

 private:
  // A counter used to assign unique IDs to web intents invocations in this
  // renderer.
  int id_counter_;

  // Map tracking registered Web Intent requests by assigned ID numbers to
  // correctly route any return data.
  std::map<int, WebKit::WebIntentRequest> intent_requests_;

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;

  // TODO(gbillock): Do we need various ***ClientRedirect methods to implement
  // intent cancelling policy? Figure this out.

  // On the service page, handler method for the IntentsMsg_SetWebIntent
  // message.
  void OnSetIntent(const webkit_glue::WebIntentData& intent);

  // On the client page, handler method for the IntentsMsg_WebIntentReply
  // message. Forwards the reply |data| to the registered WebIntentRequest
  // (found by |intent_id|), where it is dispatched to the client JS callback.
  void OnWebIntentReply(webkit_glue::WebIntentReplyType reply_type,
                        const WebKit::WebString& data,
                        int intent_id);

  // Delivered intent data from the caller.
  scoped_ptr<webkit_glue::WebIntentData> intent_;

  // The client object which will receive callback notifications from the
  // delivered intent.
  scoped_ptr<WebKit::WebDeliveredIntentClient> delivered_intent_client_;

  // If we deliver a browser-originated blob intent to the client,
  // this is that Blob.
  WebKit::WebBlob web_blob_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsHost);
};

#endif  // CONTENT_RENDERER_WEB_INTENTS_HOST_H_
