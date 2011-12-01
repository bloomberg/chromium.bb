// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/intents/intents_host_impl.h"

#include "content/browser/intents/intent_injector.h"
#include "content/common/intents_messages.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

IntentsHostImpl::IntentsHostImpl(TabContents* source_tab,
                                 const webkit_glue::WebIntentData& intent,
                                 int intent_id)
    : TabContentsObserver(source_tab),
      intent_(intent),
      intent_id_(intent_id),
      intent_injector_(NULL) {}

IntentsHostImpl::~IntentsHostImpl() {}

const webkit_glue::WebIntentData& IntentsHostImpl::GetIntent() {
  return intent_;
}

void IntentsHostImpl::DispatchIntent(TabContents* tab_contents) {
  DCHECK(!intent_injector_);
  intent_injector_ = new IntentInjector(tab_contents);
  intent_injector_->SetIntent(this, intent_, intent_id_);
}

void IntentsHostImpl::SendReplyMessage(
    webkit_glue::WebIntentReplyType reply_type,
    const string16& data) {
  intent_injector_ = NULL;

  if (!tab_contents())
    return;

  Send(new IntentsMsg_WebIntentReply(0, reply_type, data, intent_id_));
  if (!reply_notifier_.is_null())
    reply_notifier_.Run();
}

void IntentsHostImpl::RegisterReplyNotification(const base::Closure& closure) {
  reply_notifier_ = closure;
}

void IntentsHostImpl::TabContentsDestroyed(TabContents* tab) {
  if (intent_injector_)
    intent_injector_->SourceTabContentsDestroyed(tab);

  intent_injector_ = NULL;
}
