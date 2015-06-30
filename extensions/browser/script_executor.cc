// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/script_executor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/script_execution_observer.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace base {
class ListValue;
}  // namespace base

namespace extensions {

namespace {

const char* kRendererDestroyed = "The tab was closed.";

// A handler for a single injection request. On creation this will send the
// injection request to the renderer, and it will be destroyed after either the
// corresponding response comes from the renderer, or the renderer is destroyed.
class Handler : public content::WebContentsObserver {
 public:
  Handler(base::ObserverList<ScriptExecutionObserver>* script_observers,
          content::WebContents* web_contents,
          const ExtensionMsg_ExecuteCode_Params& params,
          ScriptExecutor::FrameScope scope,
          const ScriptExecutor::ExecuteScriptCallback& callback)
      : content::WebContentsObserver(web_contents),
        script_observers_(AsWeakPtr(script_observers)),
        host_id_(params.host_id),
        request_id_(params.request_id),
        callback_(callback) {
    if (scope == ScriptExecutor::ALL_FRAMES) {
      web_contents->ForEachFrame(base::Bind(&Handler::SendExecuteCode,
                                            base::Unretained(this), params));
    } else {
      SendExecuteCode(params, web_contents->GetMainFrame());
    }
  }

 private:
  // This class manages its own lifetime.
  ~Handler() override {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { Finish(); }

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    // Unpack by hand to check the request_id, since there may be multiple
    // requests in flight but only one is for this.
    if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
      return false;

    int message_request_id;
    base::PickleIterator iter(message);
    CHECK(iter.ReadInt(&message_request_id));

    if (message_request_id != request_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(Handler, message, render_frame_host)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExecuteCodeFinished,
                          OnExecuteCodeFinished)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (pending_render_frames_.erase(render_frame_host) == 1 &&
        pending_render_frames_.empty()) {
      Finish();
    }
  }

  // Sends an ExecuteCode message to the given frame host, and increments
  // the number of pending messages.
  void SendExecuteCode(const ExtensionMsg_ExecuteCode_Params& params,
                       content::RenderFrameHost* frame) {
    pending_render_frames_.insert(frame);
    frame->Send(new ExtensionMsg_ExecuteCode(frame->GetRoutingID(), params));
  }

  // Handles the ExecuteCodeFinished message.
  void OnExecuteCodeFinished(content::RenderFrameHost* render_frame_host,
                             int request_id,
                             const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& result_list) {
    DCHECK_EQ(request_id_, request_id);
    DCHECK(!pending_render_frames_.empty());
    bool erased = pending_render_frames_.erase(render_frame_host) == 1;
    DCHECK(erased);
    bool is_main_frame = web_contents()->GetMainFrame() == render_frame_host;

    // Set the result, if there is one.
    const base::Value* script_value = nullptr;
    if (result_list.Get(0u, &script_value)) {
      // If this is the main result, we put it at index 0. Otherwise, we just
      // append it at the end.
      if (is_main_frame && !results_.empty())
        CHECK(results_.Insert(0u, script_value->DeepCopy()));
      else
        results_.Append(script_value->DeepCopy());
    }

    if (is_main_frame) {  // Only use the main frame's error and url.
      main_frame_error_ = error;
      main_frame_url_ = on_url;
    }

    // Wait until the final request finishes before reporting back.
    if (pending_render_frames_.empty())
      Finish();
  }

  void Finish() {
    if (main_frame_url_.is_empty()) {
      // We never finished the main frame injection.
      main_frame_error_ = kRendererDestroyed;
      results_.Clear();
    }

    if (script_observers_.get() && main_frame_error_.empty() &&
        host_id_.type() == HostID::EXTENSIONS) {
      ScriptExecutionObserver::ExecutingScriptsMap id_map;
      id_map[host_id_.id()] = std::set<std::string>();
      FOR_EACH_OBSERVER(
          ScriptExecutionObserver, *script_observers_,
          OnScriptsExecuted(web_contents(), id_map, main_frame_url_));
    }

    if (!callback_.is_null())
      callback_.Run(main_frame_error_, main_frame_url_, results_);
    delete this;
  }

  base::WeakPtr<base::ObserverList<ScriptExecutionObserver>> script_observers_;

  // The id of the host (the extension or the webui) doing the injection.
  HostID host_id_;

  // The request id of the injection.
  int request_id_;

  // The hosts of the still-running injections.
  std::set<content::RenderFrameHost*> pending_render_frames_;

  // The results of the injection.
  base::ListValue results_;

  // The error from injecting into the main frame.
  std::string main_frame_error_;

  // The url of the main frame.
  GURL main_frame_url_;

  // The callback to run after all injections complete.
  ScriptExecutor::ExecuteScriptCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(Handler);
};

}  // namespace

ScriptExecutionObserver::~ScriptExecutionObserver() {
}

ScriptExecutor::ScriptExecutor(
    content::WebContents* web_contents,
    base::ObserverList<ScriptExecutionObserver>* script_observers)
    : next_request_id_(0),
      web_contents_(web_contents),
      script_observers_(script_observers) {
  CHECK(web_contents_);
}

ScriptExecutor::~ScriptExecutor() {
}

void ScriptExecutor::ExecuteScript(const HostID& host_id,
                                   ScriptExecutor::ScriptType script_type,
                                   const std::string& code,
                                   ScriptExecutor::FrameScope frame_scope,
                                   ScriptExecutor::MatchAboutBlank about_blank,
                                   UserScript::RunLocation run_at,
                                   ScriptExecutor::WorldType world_type,
                                   ScriptExecutor::ProcessType process_type,
                                   const GURL& webview_src,
                                   const GURL& file_url,
                                   bool user_gesture,
                                   ScriptExecutor::ResultType result_type,
                                   const ExecuteScriptCallback& callback) {
  if (host_id.type() == HostID::EXTENSIONS) {
    // Don't execute if the extension has been unloaded.
    const Extension* extension =
        ExtensionRegistry::Get(web_contents_->GetBrowserContext())
            ->enabled_extensions().GetByID(host_id.id());
    if (!extension)
      return;
  } else {
    CHECK(process_type == WEB_VIEW_PROCESS);
  }

  ExtensionMsg_ExecuteCode_Params params;
  params.request_id = next_request_id_++;
  params.host_id = host_id;
  params.is_javascript = (script_type == JAVASCRIPT);
  params.code = code;
  params.match_about_blank = (about_blank == MATCH_ABOUT_BLANK);
  params.run_at = static_cast<int>(run_at);
  params.in_main_world = (world_type == MAIN_WORLD);
  params.is_web_view = (process_type == WEB_VIEW_PROCESS);
  params.webview_src = webview_src;
  params.file_url = file_url;
  params.wants_result = (result_type == JSON_SERIALIZED_RESULT);
  params.user_gesture = user_gesture;

  // Handler handles IPCs and deletes itself on completion.
  new Handler(script_observers_, web_contents_, params, frame_scope, callback);
}

}  // namespace extensions
