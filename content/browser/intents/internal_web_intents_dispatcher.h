// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTENTS_INTERNAL_WEB_INTENTS_DISPATCHER_H_
#define CONTENT_BROWSER_INTENTS_INTERNAL_WEB_INTENTS_DISPATCHER_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "webkit/glue/web_intent_data.h"

namespace content {
class IntentInjector;

// This class implements a web intents dispatcher which originates
// within the browser process rather than with a particular renderer.
// The implementation handles replies to the web intents invocation by
// notifying a registered callback rather than returning
// those messages to any renderer.
class CONTENT_EXPORT InternalWebIntentsDispatcher
    : public WebIntentsDispatcher {
 public:
  // This callback will be called during, and receives the same args as,
  // |SendReplyMessage|.
  typedef base::Callback<void(const webkit_glue::WebIntentReply&)>
      ReplyCallback;

  // |intent| is the intent payload to be dispatched.
  explicit InternalWebIntentsDispatcher(
      const webkit_glue::WebIntentData& intent);

  // |intent| is the intent payload to be dispatched.
  // |reply_callback| is the callback to notify when the intent is replied to.
  InternalWebIntentsDispatcher(
      const webkit_glue::WebIntentData& intent,
      const ReplyCallback& reply_callback);

  virtual ~InternalWebIntentsDispatcher();

  // WebIntentsDispatcher implementation.
  virtual const webkit_glue::WebIntentData& GetIntent() OVERRIDE;
  virtual void DispatchIntent(WebContents* destination_contents) OVERRIDE;
  virtual void ResetDispatch() OVERRIDE;
  virtual void SendReply(const webkit_glue::WebIntentReply& reply) OVERRIDE;
  virtual void RegisterReplyNotification(
      const WebIntentsDispatcher::ReplyNotification& closure) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(InternalWebIntentsDispatcherTest,
                           CancelAbandonsInjector);
  // The intent data to be delivered.
  webkit_glue::WebIntentData intent_;

  // Weak pointer to the internal object which delivers the intent to the
  // newly-created service WebContents. This object is self-deleting
  // (connected to the service WebContents).
  IntentInjector* intent_injector_;

  // Callbacks to be notified when SendReplyMessage is called.
  std::vector<WebIntentsDispatcher::ReplyNotification> reply_notifiers_;

  // Callback to be invoked when the intent is replied to.
  ReplyCallback reply_callback_;

  DISALLOW_COPY_AND_ASSIGN(InternalWebIntentsDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTENTS_INTERNAL_WEB_INTENTS_DISPATCHER_H_
