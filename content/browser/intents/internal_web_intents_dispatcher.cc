// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/intents/internal_web_intents_dispatcher.h"

#include "content/browser/intents/intent_injector.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace content {

InternalWebIntentsDispatcher::InternalWebIntentsDispatcher(
    const webkit_glue::WebIntentData& intent)
    : intent_(intent),
      intent_injector_(NULL) {}

InternalWebIntentsDispatcher::InternalWebIntentsDispatcher(
    const webkit_glue::WebIntentData& intent,
    const ReplyCallback& reply_callback)
    : intent_(intent),
      intent_injector_(NULL),
      reply_callback_(reply_callback) {}

InternalWebIntentsDispatcher::~InternalWebIntentsDispatcher() {}

const webkit_glue::WebIntentData& InternalWebIntentsDispatcher::GetIntent() {
  return intent_;
}

void InternalWebIntentsDispatcher::DispatchIntent(
    WebContents* destination_contents) {
  DCHECK(destination_contents);
  DCHECK(!intent_injector_);
  intent_injector_ = new IntentInjector(destination_contents);
  intent_injector_->SetIntent(this, intent_);
}

void InternalWebIntentsDispatcher::ResetDispatch() {
  if (intent_injector_) {
    intent_injector_->Abandon();
    intent_injector_ = NULL;
  }
}

void InternalWebIntentsDispatcher::SendReply(
    const webkit_glue::WebIntentReply& reply) {
  intent_injector_ = NULL;

  for (size_t i = 0; i < reply_notifiers_.size(); ++i) {
    if (!reply_notifiers_[i].is_null())
      reply_notifiers_[i].Run(reply.type);
  }

  // Notify the callback of the reply.
  if (!reply_callback_.is_null())
    reply_callback_.Run(reply);

  delete this;
}

void InternalWebIntentsDispatcher::RegisterReplyNotification(
    const WebIntentsDispatcher::ReplyNotification& closure) {
  reply_notifiers_.push_back(closure);
}

}  // namespace content
