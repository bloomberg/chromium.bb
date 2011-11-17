// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/user_script_idle_scheduler.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace {
// The length of time to wait after the DOM is complete to try and run user
// scripts.
const int kUserScriptIdleTimeoutMs = 200;
}

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebView;

UserScriptIdleScheduler::UserScriptIdleScheduler(
    WebFrame* frame, ExtensionDispatcher* extension_dispatcher)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      frame_(frame),
      has_run_(false),
      extension_dispatcher_(extension_dispatcher) {
}

UserScriptIdleScheduler::~UserScriptIdleScheduler() {
}

void UserScriptIdleScheduler::ExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params) {
  if (!has_run_) {
    pending_code_execution_queue_.push(
        linked_ptr<ExtensionMsg_ExecuteCode_Params>(
            new ExtensionMsg_ExecuteCode_Params(params)));
    return;
  }

  ExecuteCodeImpl(params);
}

void UserScriptIdleScheduler::DidFinishDocumentLoad() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&UserScriptIdleScheduler::MaybeRun,
                            weak_factory_.GetWeakPtr()),
      kUserScriptIdleTimeoutMs);
}

void UserScriptIdleScheduler::DidFinishLoad() {
  // Ensure that running scripts does not keep any progress UI running.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&UserScriptIdleScheduler::MaybeRun,
                            weak_factory_.GetWeakPtr()));
}

void UserScriptIdleScheduler::DidStartProvisionalLoad() {
  // The frame is navigating, so reset the state since we'll want to inject
  // scripts once the load finishes.
  has_run_ = false;
  weak_factory_.InvalidateWeakPtrs();
  while (!pending_code_execution_queue_.empty())
    pending_code_execution_queue_.pop();
}

void UserScriptIdleScheduler::MaybeRun() {
  if (has_run_)
    return;

  // Note: we must set this before calling ExecuteCodeImpl, because that may
  // result in a synchronous call back into MaybeRun if there is a pending task
  // currently in the queue.
  // http://code.google.com/p/chromium/issues/detail?id=29644
  has_run_ = true;

  extension_dispatcher_->user_script_slave()->InjectScripts(
      frame_, UserScript::DOCUMENT_IDLE);

  while (!pending_code_execution_queue_.empty()) {
    linked_ptr<ExtensionMsg_ExecuteCode_Params>& params =
        pending_code_execution_queue_.front();
    ExecuteCodeImpl(*params);
    pending_code_execution_queue_.pop();
  }
}

void UserScriptIdleScheduler::ExecuteCodeImpl(
    const ExtensionMsg_ExecuteCode_Params& params) {
  const Extension* extension = extension_dispatcher_->extensions()->GetByID(
      params.extension_id);
  content::RenderView* render_view =
      content::RenderView::FromWebView(frame_->view());

  // Since extension info is sent separately from user script info, they can
  // be out of sync. We just ignore this situation.
  if (!extension) {
    render_view->Send(new ExtensionHostMsg_ExecuteCodeFinished(
        render_view->GetRoutingId(), params.request_id, true, ""));
    return;
  }

  std::vector<WebFrame*> frame_vector;
  frame_vector.push_back(frame_);
  if (params.all_frames)
    GetAllChildFrames(frame_, &frame_vector);

  for (std::vector<WebFrame*>::iterator frame_it = frame_vector.begin();
       frame_it != frame_vector.end(); ++frame_it) {
    WebFrame* frame = *frame_it;
    if (params.is_javascript) {
      // We recheck access here in the renderer for extra safety against races
      // with navigation.
      //
      // But different frames can have different URLs, and the extension might
      // only have access to a subset of them. For the top frame, we can
      // immediately send an error and stop because the browser process
      // considers that an error too.
      //
      // For child frames, we just skip ones the extension doesn't have access
      // to and carry on.
      if (!extension->CanExecuteScriptOnPage(frame->document().url(),
                                             NULL, NULL)) {
        if (frame->parent()) {
          continue;
        } else {
          render_view->Send(new ExtensionHostMsg_ExecuteCodeFinished(
              render_view->GetRoutingId(), params.request_id, false,
              ExtensionErrorUtils::FormatErrorMessage(
                  extension_manifest_errors::kCannotAccessPage,
                  frame->document().url().spec())));
          return;
        }
      }

      WebScriptSource source(WebString::fromUTF8(params.code));
      if (params.in_main_world) {
        frame->executeScript(source);
      } else {
        std::vector<WebScriptSource> sources;
        sources.push_back(source);
        frame->executeScriptInIsolatedWorld(
            extension_dispatcher_->user_script_slave()->
                GetIsolatedWorldIdForExtension(extension, frame),
            &sources.front(), sources.size(), EXTENSION_GROUP_CONTENT_SCRIPTS);
      }
    } else {
      frame->document().insertUserStyleSheet(
          WebString::fromUTF8(params.code),
          // Author level is consistent with WebView::addUserStyleSheet.
          WebDocument::UserStyleAuthorLevel);
    }
  }

  render_view->Send(new ExtensionHostMsg_ExecuteCodeFinished(
      render_view->GetRoutingId(), params.request_id, true, ""));
}

bool UserScriptIdleScheduler::GetAllChildFrames(
    WebFrame* parent_frame,
    std::vector<WebFrame*>* frames_vector) const {
  if (!parent_frame)
    return false;

  for (WebFrame* child_frame = parent_frame->firstChild(); child_frame;
       child_frame = child_frame->nextSibling()) {
    frames_vector->push_back(child_frame);
    GetAllChildFrames(child_frame, frames_vector);
  }
  return true;
}
