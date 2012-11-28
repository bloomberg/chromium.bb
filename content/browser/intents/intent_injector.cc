// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/intents/intent_injector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/intents/web_intents_dispatcher_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/intents_messages.h"
#include "content/public/common/content_switches.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace content {

IntentInjector::IntentInjector(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      intents_dispatcher_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(web_contents);
}

IntentInjector::~IntentInjector() {
}

void IntentInjector::WebContentsDestroyed(WebContents* contents) {
  if (intents_dispatcher_) {
    intents_dispatcher_->SendReply(webkit_glue::WebIntentReply(
        webkit_glue::WEB_INTENT_SERVICE_CONTENTS_CLOSED, string16()));
  }

  delete this;
}

void IntentInjector::SourceWebContentsDestroyed(WebContents* contents) {
  intents_dispatcher_ = NULL;
}

void IntentInjector::SetIntent(
    WebIntentsDispatcher* intents_dispatcher,
    const webkit_glue::WebIntentData& intent) {
  intents_dispatcher_ = intents_dispatcher;
  intents_dispatcher_->RegisterReplyNotification(
      base::Bind(&IntentInjector::OnSendReturnMessage,
                 weak_factory_.GetWeakPtr()));
  source_intent_.reset(new webkit_glue::WebIntentData(intent));
  initial_url_ = web_contents()->GetPendingSiteInstance()->GetSiteURL();
}

void IntentInjector::Abandon() {
  intents_dispatcher_ = NULL;
  delete this;
}

void IntentInjector::OnSendReturnMessage(
    webkit_glue::WebIntentReplyType reply_type) {
  intents_dispatcher_ = NULL;
}

void IntentInjector::RenderViewCreated(RenderViewHost* render_view_host) {
  if (source_intent_.get() == NULL || !web_contents()->GetRenderViewHost())
    return;

  // Only deliver the intent to the renderer if it has the same origin
  // as the initial delivery target.
  if (initial_url_.GetOrigin() !=
      render_view_host->GetSiteInstance()->GetSiteURL().GetOrigin()) {
    return;
  }

  // If we're passing a browser-originated blob, either directly or as part of a
  // payload, grant read permission on the blob file to the delivered context.
  if (source_intent_->data_type == webkit_glue::WebIntentData::BLOB ||
      (source_intent_->data_type == webkit_glue::WebIntentData::MIME_TYPE &&
       !source_intent_->blob_file.empty())) {
    const int child_id = render_view_host->GetProcess()->GetID();
    ChildProcessSecurityPolicy* policy =
         ChildProcessSecurityPolicy::GetInstance();
    if (!policy->CanReadFile(child_id, source_intent_->blob_file))
      policy->GrantReadFile(child_id, source_intent_->blob_file);
  } else if (source_intent_->data_type ==
             webkit_glue::WebIntentData::FILESYSTEM) {
    const int child_id = render_view_host->GetProcess()->GetID();
    FilePath path;
    const bool valid =
        fileapi::IsolatedContext::GetInstance()->GetRegisteredPath(
            source_intent_->filesystem_id, &path);
    DCHECK(valid);
    ChildProcessSecurityPolicy* policy =
         ChildProcessSecurityPolicy::GetInstance();
    if (!policy->CanReadFile(child_id, path))
      policy->GrantReadFile(child_id, path);
    policy->GrantReadFileSystem(child_id, source_intent_->filesystem_id);
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

void IntentInjector::OnReply(const webkit_glue::WebIntentReply& reply) {
  if (!intents_dispatcher_)
    return;

  // Ensure SendReplyMessage is only called once.
  WebIntentsDispatcher* intents_dispatcher = intents_dispatcher_;
  intents_dispatcher_ = NULL;
  intents_dispatcher->SendReply(reply);
}

}  // namespace content
