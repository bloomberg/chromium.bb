// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/intents/intent_injector.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

IntentInjector::IntentInjector(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      intent_id_(0) {
  DCHECK(tab_contents);
}

IntentInjector::~IntentInjector() {
}

void IntentInjector::TabContentsDestroyed(TabContents* tab) {
  if (source_tab_.get() != NULL) {
    scoped_ptr<IPC::Message::Sender> sender;
    sender.swap(source_tab_);
    sender->Send(new IntentsMsg_WebIntentReply(
        0, webkit_glue::WEB_INTENT_SERVICE_TAB_CLOSED, string16(), intent_id_));
  }

  delete this;
}

void IntentInjector::SourceTabContentsDestroyed(TabContents* tab) {
  source_tab_.reset(NULL);
}

void IntentInjector::SetIntent(IPC::Message::Sender* source_tab,
                               const webkit_glue::WebIntentData& intent,
                               int intent_id) {
  source_tab_.reset(source_tab);
  source_intent_.reset(new webkit_glue::WebIntentData(intent));
  intent_id_ = intent_id;

  SendIntent();
}

void IntentInjector::RenderViewCreated(RenderViewHost* host) {
  SendIntent();
}

void IntentInjector::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
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
      tab_contents()->render_view_host() == NULL) {
    return;
  }

  // Send intent data through to renderer.
  tab_contents()->render_view_host()->Send(new IntentsMsg_SetWebIntentData(
      tab_contents()->render_view_host()->routing_id(),
      *(source_intent_.get()),
      intent_id_));
  source_intent_.reset(NULL);
}

bool IntentInjector::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IntentInjector, message)
    IPC_MESSAGE_HANDLER(IntentsMsg_WebIntentReply, OnReply);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IntentInjector::OnReply(const IPC::Message& message,
                             webkit_glue::WebIntentReplyType reply_type,
                             const string16& data,
                             int intent_id) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableWebIntents))
    NOTREACHED();

  if (source_tab_.get() != NULL) {
    // Swap before use since sending this message may cause
    // TabContentsDestroyed to be called.
    scoped_ptr<IPC::Message::Sender> sender;
    sender.swap(source_tab_);
    sender->Send(new IntentsMsg_WebIntentReply(
        0, reply_type, data, intent_id));
  }
}
