// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/user_script_scheduler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/dom_activity_logger.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace {
// The length of time to wait after the DOM is complete to try and run user
// scripts.
const int kUserScriptIdleTimeoutMs = 200;
}

using blink::WebDocument;
using blink::WebFrame;
using blink::WebString;
using blink::WebVector;
using blink::WebView;

namespace extensions {

UserScriptScheduler::UserScriptScheduler(WebFrame* frame,
                                         Dispatcher* dispatcher)
    : weak_factory_(this),
      frame_(frame),
      current_location_(UserScript::UNDEFINED),
      has_run_idle_(false),
      dispatcher_(dispatcher) {
  for (int i = UserScript::UNDEFINED; i < UserScript::RUN_LOCATION_LAST; ++i) {
    pending_execution_map_[static_cast<UserScript::RunLocation>(i)] =
      std::queue<linked_ptr<ExtensionMsg_ExecuteCode_Params> >();
  }
}

UserScriptScheduler::~UserScriptScheduler() {
}

void UserScriptScheduler::ExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params) {
  UserScript::RunLocation run_at =
    static_cast<UserScript::RunLocation>(params.run_at);
  if (current_location_ < run_at) {
    pending_execution_map_[run_at].push(
        linked_ptr<ExtensionMsg_ExecuteCode_Params>(
            new ExtensionMsg_ExecuteCode_Params(params)));
    return;
  }

  ExecuteCodeImpl(params);
}

void UserScriptScheduler::DidCreateDocumentElement() {
  current_location_ = UserScript::DOCUMENT_START;
  MaybeRun();
}

void UserScriptScheduler::DidFinishDocumentLoad() {
  current_location_ = UserScript::DOCUMENT_END;
  MaybeRun();
  // Schedule a run for DOCUMENT_IDLE
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&UserScriptScheduler::IdleTimeout, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUserScriptIdleTimeoutMs));
}

void UserScriptScheduler::DidFinishLoad() {
  current_location_ = UserScript::DOCUMENT_IDLE;
  // Ensure that running scripts does not keep any progress UI running.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&UserScriptScheduler::MaybeRun, weak_factory_.GetWeakPtr()));
}

void UserScriptScheduler::DidStartProvisionalLoad() {
  // The frame is navigating, so reset the state since we'll want to inject
  // scripts once the load finishes.
  current_location_ = UserScript::UNDEFINED;
  has_run_idle_ = false;
  weak_factory_.InvalidateWeakPtrs();
  std::map<UserScript::RunLocation, ExecutionQueue>::iterator itr =
    pending_execution_map_.begin();
  for (itr = pending_execution_map_.begin();
       itr != pending_execution_map_.end(); ++itr) {
    while (!itr->second.empty())
      itr->second.pop();
  }
}

void UserScriptScheduler::IdleTimeout() {
  current_location_ = UserScript::DOCUMENT_IDLE;
  MaybeRun();
}

void UserScriptScheduler::MaybeRun() {
  if (current_location_ == UserScript::UNDEFINED)
    return;

  if (!has_run_idle_ && current_location_ == UserScript::DOCUMENT_IDLE) {
    has_run_idle_ = true;
    dispatcher_->user_script_slave()->InjectScripts(
        frame_, UserScript::DOCUMENT_IDLE);
  }

  // Run all tasks from the current time and earlier.
  for (int i = UserScript::DOCUMENT_START;
       i <= current_location_; ++i) {
    UserScript::RunLocation run_time = static_cast<UserScript::RunLocation>(i);
    while (!pending_execution_map_[run_time].empty()) {
      linked_ptr<ExtensionMsg_ExecuteCode_Params>& params =
        pending_execution_map_[run_time].front();
      ExecuteCodeImpl(*params);
      pending_execution_map_[run_time].pop();
    }
  }
}

void UserScriptScheduler::ExecuteCodeImpl(
    const ExtensionMsg_ExecuteCode_Params& params) {
  const Extension* extension = dispatcher_->extensions()->GetByID(
      params.extension_id);
  content::RenderView* render_view =
      content::RenderView::FromWebView(frame_->view());
  ExtensionHelper* extension_helper = ExtensionHelper::Get(render_view);
  base::ListValue execution_results;

  // Since extension info is sent separately from user script info, they can
  // be out of sync. We just ignore this situation.
  if (!extension) {
    render_view->Send(
        new ExtensionHostMsg_ExecuteCodeFinished(render_view->GetRoutingID(),
                                                 params.request_id,
                                                 std::string(),  // no error
                                                 -1,
                                                 GURL(std::string()),
                                                 execution_results));
    return;
  }

  std::vector<WebFrame*> frame_vector;
  frame_vector.push_back(frame_);
  if (params.all_frames)
    GetAllChildFrames(frame_, &frame_vector);

  std::string error;

  scoped_ptr<blink::WebScopedUserGesture> gesture;
  if (params.user_gesture)
    gesture.reset(new blink::WebScopedUserGesture);

  GURL top_url = frame_->document().url();

  for (std::vector<WebFrame*>::iterator frame_it = frame_vector.begin();
       frame_it != frame_vector.end(); ++frame_it) {
    WebFrame* child_frame = *frame_it;
    CHECK(child_frame) << top_url;

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

    bool can_execute_script =
        PermissionsData::CanExecuteScriptOnPage(extension,
                                                child_frame->document().url(),
                                                top_url,
                                                extension_helper->tab_id(),
                                                NULL,
                                                -1,
                                                NULL);
    if ((!params.is_web_view && !can_execute_script) ||
        (params.is_web_view &&
         child_frame->document().url() != params.webview_src)) {
      if (child_frame->parent()) {
        continue;
      } else {
        error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessPage,
            child_frame->document().url().spec());
        break;
      }
    }

    if (params.is_javascript) {
      WebScriptSource source(WebString::fromUTF8(params.code), params.file_url);
      v8::HandleScope scope(v8::Isolate::GetCurrent());

      scoped_ptr<content::V8ValueConverter> v8_converter(
          content::V8ValueConverter::create());
      v8::Local<v8::Value> script_value;

      if (params.in_main_world) {
        DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId,
                                         extension->id());
        script_value = child_frame->executeScriptAndReturnValue(source);
      } else {
        blink::WebVector<v8::Local<v8::Value> > results;
        std::vector<WebScriptSource> sources;
        sources.push_back(source);
        int isolated_world_id =
            dispatcher_->user_script_slave()->GetIsolatedWorldIdForExtension(
                extension, child_frame);
        DOMActivityLogger::AttachToWorld(isolated_world_id, extension->id());
        child_frame->executeScriptInIsolatedWorld(
            isolated_world_id, &sources.front(),
            sources.size(), EXTENSION_GROUP_CONTENT_SCRIPTS, &results);
        // We only expect one value back since we only pushed one source
        if (results.size() == 1 && !results[0].IsEmpty())
          script_value = results[0];
      }

      if (params.wants_result && !script_value.IsEmpty()) {
        // It's safe to always use the main world context when converting here.
        // V8ValueConverterImpl shouldn't actually care about the context scope,
        // and it switches to v8::Object's creation context when encountered.
        v8::Local<v8::Context> context = child_frame->mainWorldScriptContext();
        base::Value* result = v8_converter->FromV8Value(script_value, context);
        // Always append an execution result (i.e. no result == null result) so
        // that |execution_results| lines up with the frames.
        execution_results.Append(
            result ? result : base::Value::CreateNullValue());
      }
    } else {
      child_frame->document().insertStyleSheet(
          WebString::fromUTF8(params.code));
    }
  }

  render_view->Send(new ExtensionHostMsg_ExecuteCodeFinished(
      render_view->GetRoutingID(),
      params.request_id,
      error,
      render_view->GetPageId(),
      UserScriptSlave::GetDataSourceURLForFrame(frame_),
      execution_results));
}

bool UserScriptScheduler::GetAllChildFrames(
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

}  // namespace extensions
