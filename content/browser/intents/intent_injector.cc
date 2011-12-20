// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/intents/intent_injector.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/intents_messages.h"
#include "content/public/browser/intents_host.h"
#include "content/public/common/content_switches.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

IntentInjector::IntentInjector(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      source_tab_(NULL) {
  DCHECK(tab_contents);
}

IntentInjector::~IntentInjector() {
}

void IntentInjector::TabContentsDestroyed(TabContents* tab) {
  if (source_tab_) {
    source_tab_->SendReplyMessage(webkit_glue::WEB_INTENT_SERVICE_TAB_CLOSED,
                                  string16());
  }

  delete this;
}

void IntentInjector::SourceTabContentsDestroyed(TabContents* tab) {
  source_tab_ = NULL;
}

void IntentInjector::SetIntent(content::IntentsHost* source_tab,
                               const webkit_glue::WebIntentData& intent) {
  source_tab_ = source_tab;
  source_intent_.reset(new webkit_glue::WebIntentData(intent));

  SendIntent();
}

void IntentInjector::RenderViewCreated(RenderViewHost* host) {
  SendIntent();
}

void IntentInjector::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  SendIntent();
}

// TODO(gbillock): The "correct" thing here is for this to be a
// RenderViewHostObserver, and do this on RenderViewHostInitialized. There's no
// good hooks for attaching the intent to such an object, though. All RVHOs get
// made deep inside tab contents initialization. Idea: propagate out
// RenderViewHostInitialized to a TabContentsObserver latch? That still looks
// like it might be racy, though.
void IntentInjector::SendIntent() {
  if (source_intent_.get() == NULL ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebIntents) ||
      tab_contents()->GetRenderViewHost() == NULL) {
    return;
  }

  // Send intent data through to renderer.
  tab_contents()->GetRenderViewHost()->Send(new IntentsMsg_SetWebIntentData(
      tab_contents()->GetRenderViewHost()->routing_id(),
      *(source_intent_.get())));
  source_intent_.reset(NULL);
}

bool IntentInjector::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IntentInjector, message)
    IPC_MESSAGE_HANDLER(IntentsHostMsg_WebIntentReply, OnReply);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IntentInjector::OnReply(webkit_glue::WebIntentReplyType reply_type,
                             const string16& data) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableWebIntents))
    NOTREACHED();

  if (source_tab_) {
    source_tab_->SendReplyMessage(reply_type, data);
  }
}
