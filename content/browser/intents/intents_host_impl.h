// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTENTS_INTENTS_HOST_IMPL_H_
#define CONTENT_BROWSER_INTENTS_INTENTS_HOST_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/intents_host.h"
#include "webkit/glue/web_intent_data.h"

class IntentInjector;

// Implements the coordinator object interface for Web Intents.
// Observes the source (client) tab to make sure messages sent back by the
// service can be delivered. Keeps a copy of triggering intent data to
// be delivered to the service and serves as a forwarder for sending reply
// messages back to the client page.
class IntentsHostImpl : public content::IntentsHost,
                        public TabContentsObserver {
 public:
  // |source_tab| is the page which triggered the web intent.
  // |intent| is the intent payload created by that page.
  // |intent_id| is the identifier assigned by WebKit to direct replies back to
  // the correct Javascript callback.
  IntentsHostImpl(TabContents* source_tab,
                  const webkit_glue::WebIntentData& intent,
                  int intent_id);
  virtual ~IntentsHostImpl();

  // IntentsHost implementation
  virtual const webkit_glue::WebIntentData& GetIntent() OVERRIDE;
  virtual void DispatchIntent(TabContents* tab_contents) OVERRIDE;
  virtual void SendReplyMessage(webkit_glue::WebIntentReplyType reply_type,
                                const string16& data) OVERRIDE;
  virtual void RegisterReplyNotification(const base::Closure& closure) OVERRIDE;

  // TabContentsObserver implementation
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

 private:
  webkit_glue::WebIntentData intent_;

  int intent_id_;

  // Weak pointer to the internal object which provides the intent to the
  // newly-created service tab contents. This object is self-deleting
  // (connected to the service TabContents).
  IntentInjector* intent_injector_;

  // A callback to be notified when SendReplyMessage is called.
  base::Closure reply_notifier_;

  DISALLOW_COPY_AND_ASSIGN(IntentsHostImpl);
};

#endif  // CONTENT_BROWSER_INTENTS_INTENTS_HOST_IMPL_H_
