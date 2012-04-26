// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_INTENTS_DISPATCHER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_INTENTS_DISPATCHER_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace webkit_glue {
enum WebIntentReplyType;
struct WebIntentData;
}

namespace content {

class WebContents;
class WebContentsDelegate;

// This class is the coordinator for dispatching web intents and seeing that
// return messages are sent to the correct invoking context. The WebContents
// for the invoking context will create one of these for each intent and hand
// a pointer to the client WebContentsDelegate code. The WebContentsDelegate
// code can then read the intent data, create UI to pick the service, and
// create a new context for that service.
//
// At that point, it should call DispatchIntent, which will deliver the intent
// to the new context. If anything goes wrong during setup, the client
// should call SendReplyMessage with an error. The dispatcher lives until the
// SendReplyMessage method is called, which will self-delete the object.
//
// The client should also register a reply notification, so it can avoid
// referencing the dispatcher after other code calls SendReplyMessage, which can
// happen if, for example, the user closes the delivery context.
class CONTENT_EXPORT WebIntentsDispatcher {
 public:
  // This callback type is registered for notification of |SendReplyMessage|.
  typedef base::Callback<void(webkit_glue::WebIntentReplyType)>
      ReplyNotification;

  // Create internal (browser-triggered) intent. This will create
  // a new dispatcher with the passed intent payload |data|. The caller should
  // manage dispatching it correctly.
  static WebIntentsDispatcher* Create(const webkit_glue::WebIntentData& data);

  virtual ~WebIntentsDispatcher() {}

  // Get the intent data being dispatched.
  virtual const webkit_glue::WebIntentData& GetIntent() = 0;

  // Attach the intent to a new context in which the service page is loaded.
  // |web_contents| must not be NULL.
  virtual void DispatchIntent(WebContents* web_contents) = 0;

  // Abandon current attempt to dispatch, allow new call to DispatchIntent.
  virtual void ResetDispatch() = 0;

  // Return a success or failure message to the source context which invoked
  // the intent. Deletes the object; it should not be used after this call
  // returns. Calls the reply notifications, if any are registered.
  virtual void SendReplyMessage(webkit_glue::WebIntentReplyType reply_type,
                                const string16& data) = 0;

  // Register a callback to be notified when SendReplyMessage is called.
  // Multiple callbacks may be registered.
  virtual void RegisterReplyNotification(const ReplyNotification& closure) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_INTENTS_DISPATCHER_H_
