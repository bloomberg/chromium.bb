// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
#define CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/intents_messages.h"

class RenderViewHost;
class TabContents;

namespace webkit_glue {
struct WebIntentData;
}

namespace IPC {
class Message;
}

// Injects an intent into the renderer of a TabContents. The intent dispatch
// logic will create one of these to take care of passing intent data down into
// the context of the service, which will be running in the TabContents on which
// this class is an observer. Deletes itself when the tab is closed.
class IntentInjector : public TabContentsObserver {
 public:
  // |tab_contents| must not be NULL.
  explicit IntentInjector(TabContents* tab_contents);
  virtual ~IntentInjector();

  // TabContentsObserver implementation.
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      const content::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

  // Sets the intent data to be injected. Call after the user has selected a
  // service to pass the intent data to that service.
  void SetIntent(int routing_id,
                 const webkit_glue::WebIntentData& intent,
                 int intent_id);

 private:
  // Delivers the intent data to the renderer.
  void SendIntent();

  // Handles receiving a reply from the intent delivery page.
  void OnReply(const IPC::Message& message,
               IntentsMsg_WebIntentReply_Type::Value reply_type,
               const string16& data,
               int intent_id);

  // Source intent data provided by caller.
  scoped_ptr<webkit_glue::WebIntentData> source_intent_;
  int source_routing_id_;
  int intent_id_;

  DISALLOW_COPY_AND_ASSIGN(IntentInjector);
};

#endif  // CONTENT_BROWSER_INTENTS_INTENT_INJECTOR_H_
