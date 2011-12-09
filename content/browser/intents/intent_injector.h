// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
#define CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/content_export.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace content {
class IntentsHost;
}

namespace webkit_glue {
struct WebIntentData;
}

// Injects an intent into the renderer of a TabContents. The intent dispatch
// logic will create one of these to take care of passing intent data down into
// the context of the service, which will be running in the TabContents on which
// this class is an observer. Attaches to the service tab and deletes itself
// when that TabContents is closed.
class CONTENT_EXPORT IntentInjector : public TabContentsObserver {
 public:
  // |tab_contents| must not be NULL.
  explicit IntentInjector(TabContents* tab_contents);
  virtual ~IntentInjector();

  // TabContentsObserver implementation.
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

  // Used to notify the object that the source tab has been destroyed.
  virtual void SourceTabContentsDestroyed(TabContents* tab);

  // Sets the intent data to be injected. Call after the user has selected a
  // service to pass the intent data to that service.
  // |source_tab| is a sender to use to communicate to the source tab. The
  // caller must ensure that SourceTabContentsDestroyed is called when this
  // object becomes unusable.
  // |intent| is the intent data from the source
  void SetIntent(content::IntentsHost* source_tab,
                 const webkit_glue::WebIntentData& intent);

 private:
  // Delivers the intent data to the renderer.
  void SendIntent();

  // Handles receiving a reply from the intent delivery page.
  void OnReply(webkit_glue::WebIntentReplyType reply_type,
               const string16& data);

  // Source intent data provided by caller.
  scoped_ptr<webkit_glue::WebIntentData> source_intent_;

  // Weak pointer to the message forwarder to the tab invoking the intent.
  content::IntentsHost* source_tab_;

  DISALLOW_COPY_AND_ASSIGN(IntentInjector);
};

#endif  // CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
