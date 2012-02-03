// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_INTENTS_DISPATCHER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_INTENTS_DISPATCHER_H_

#include "base/callback.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace webkit_glue {
enum WebIntentReplyType;
struct WebIntentData;
}

namespace content {

class WebContents;

// This class is the coordinator for dispatching web intents and seeing that
// return messages are sent to the correct invoking context. The WebContents
// for the invoking context will create one of these for each intent and hand
// a pointer to the client WebContentsDelegate code. The WebContentsDelegate
// code can then read the intent data, create UI to pick the service, and
// create a new context for that service.
//
// At that point, it should call DispatchIntent, which will connect the object
// to the new context. If anything goes wrong, the client should call
// SendReplyMessage with an error. That will self-delete the object.
//
// At that point, before the client may use the object again, it must register a
// reply notification, so it can avoid referencing the dispatcher after other
// code calls SendReplyMessage.
class CONTENT_EXPORT WebIntentsDispatcher {
 public:
  virtual ~WebIntentsDispatcher() {}

  // Get the intent data being dispatched.
  virtual const webkit_glue::WebIntentData& GetIntent() = 0;

  // Attach the intent to a new context in which the service page is loaded.
  virtual void DispatchIntent(WebContents* web_contents) = 0;

  // Return a success or failure message to the source context which invoked
  // the intent. Deletes the object; it should not be used after this call
  // returns. Calls the reply notification, if registered.
  virtual void SendReplyMessage(webkit_glue::WebIntentReplyType reply_type,
                                const string16& data) = 0;

  // Register a callback to be notified when SendReplyMessage is called.
  virtual void RegisterReplyNotification(
      const base::Callback<void(webkit_glue::WebIntentReplyType)>& closure) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_INTENTS_DISPATCHER_H_
