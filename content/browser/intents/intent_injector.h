// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
#define CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace content {
class WebIntentsDispatcher;
}

namespace webkit_glue {
struct WebIntentData;
}

// Injects an intent into the renderer of a WebContents. The intent dispatch
// logic will create one of these to take care of passing intent data down into
// the context of the service, which will be running in the WebContents on which
// this class is an observer. Attaches to the service contents and deletes
// itself when that WebContents is closed.
//
// This object should be attached to the new WebContents very early: before the
// RenderView is created. It will then send the intent data down to the renderer
// on the RenderViewCreated call, so that the intent data is available
// throughout the parsing of the loaded document.
class CONTENT_EXPORT IntentInjector : public content::WebContentsObserver {
 public:
  // |web_contents| must not be NULL.
  explicit IntentInjector(content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE;

  // Used to notify the object that the source contents has been destroyed.
  void SourceWebContentsDestroyed(content::WebContents* contents);

  // Sets the intent data to be injected. Call after the user has selected a
  // service to pass the intent data to that service. |intents_dispatcher| is a
  // sender to use to communicate to the source contents. The caller must
  // ensure that SourceWebContentsDestroyed is called when this object becomes
  // unusable. |intent| is the intent data from the source.
  void SetIntent(content::WebIntentsDispatcher* intents_dispatcher,
                 const webkit_glue::WebIntentData& intent);

  // Abandon this injector. Used when re-delivering an intent to a different
  // service provider, i.e. when abandoning an inline disposition and picking
  // a new provider. Self-deletes.
  void Abandon();

 private:
  FRIEND_TEST_ALL_PREFIXES(IntentInjectorTest, AbandonDeletes);

  // Private, since the class is self-deleting.
  virtual ~IntentInjector();

  // Handles receiving a reply from the intent delivery page.
  void OnReply(webkit_glue::WebIntentReplyType reply_type,
               const string16& data);

  // Gets notification that someone else has replied to the intent call.
  void OnSendReturnMessage(webkit_glue::WebIntentReplyType reply_type);

  // Source intent data provided by caller.
  scoped_ptr<webkit_glue::WebIntentData> source_intent_;

  // Weak pointer to the message forwarder to the contents invoking the intent.
  content::WebIntentsDispatcher* intents_dispatcher_;

  // Remember the initial delivery url for origin restriction.
  GURL initial_url_;

  // The injector may be Abandon()ed and deleted before the dispatcher. Use weak
  // pointers to ensure the notification callback remains valid.
  base::WeakPtrFactory<IntentInjector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IntentInjector);
};

#endif  // CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
