// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/intents/intent_injector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/intents_messages.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/content_switches.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

using content::RenderViewHost;
using content::WebContents;

IntentInjector::IntentInjector(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      intents_dispatcher_(NULL) {
  DCHECK(web_contents);
}

IntentInjector::~IntentInjector() {
}

void IntentInjector::WebContentsDestroyed(content::WebContents* tab) {
  if (intents_dispatcher_) {
    intents_dispatcher_->SendReplyMessage(
        webkit_glue::WEB_INTENT_SERVICE_TAB_CLOSED, string16());
  }

  delete this;
}

void IntentInjector::SourceWebContentsDestroyed(WebContents* tab) {
  intents_dispatcher_ = NULL;
}

void IntentInjector::SetIntent(
    content::WebIntentsDispatcher* intents_dispatcher,
    const webkit_glue::WebIntentData& intent) {
  intents_dispatcher_ = intents_dispatcher;
  intents_dispatcher_->RegisterReplyNotification(
      base::Bind(&IntentInjector::OnSendReturnMessage, base::Unretained(this)));
  source_intent_.reset(new webkit_glue::WebIntentData(intent));
  initial_url_ = web_contents()->GetPendingSiteInstance()->GetSite();
}

void IntentInjector::OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
  intents_dispatcher_ = NULL;
}

void IntentInjector::RenderViewCreated(RenderViewHost* render_view_host) {
  if (source_intent_.get() == NULL ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebIntents) ||
      web_contents()->GetRenderViewHost() == NULL) {
    return;
  }

  // Only deliver the intent to the renderer if it has the same origin
  // as the initial delivery target.
  if (initial_url_.GetOrigin() !=
      render_view_host->GetSiteInstance()->GetSite().GetOrigin()) {
    return;
  }

  render_view_host->Send(new IntentsMsg_SetWebIntentData(
      render_view_host->GetRoutingID(), *(source_intent_.get())));
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableWebIntents))
    NOTREACHED();

  if (intents_dispatcher_) {
    // Ensure we only call SendReplyMessage once.
    content::WebIntentsDispatcher* intents_dispatcher = intents_dispatcher_;
    intents_dispatcher_ = NULL;
    intents_dispatcher->SendReplyMessage(reply_type, data);
  }
}
