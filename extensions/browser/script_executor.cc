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
  Handler(ObserverList<ScriptExecutionObserver>* script_observers,
          content::WebContents* web_contents,
          ExtensionMsg_ExecuteCode_Params* params,
          ScriptExecutor::FrameScope scope,
          int* request_id_counter,
          const ScriptExecutor::ExecuteScriptCallback& callback)
      : content::WebContentsObserver(web_contents),
        script_observers_(AsWeakPtr(script_observers)),
        host_id_(params->host_id),
        main_request_id_((*request_id_counter)++),
        sub_request_id_(scope == ScriptExecutor::ALL_FRAMES ?
                            (*request_id_counter)++ : -1),
        num_pending_(0),
        callback_(callback) {
    content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
    if (scope == ScriptExecutor::ALL_FRAMES) {
      web_contents->ForEachFrame(base::Bind(&Handler::SendExecuteCode,
                                            base::Unretained(this), params,
                                            main_frame));
    } else {
      SendExecuteCode(params, main_frame, main_frame);
    }
  }

 private:
  // This class manages its own lifetime.
  ~Handler() override {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    Finish(kRendererDestroyed, GURL(), base::ListValue());
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    // Unpack by hand to check the request_id, since there may be multiple
    // requests in flight but only one is for this.
    if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
      return false;

    int message_request_id;
    base::PickleIterator iter(message);
    CHECK(iter.ReadInt(&message_request_id));

    if (message_request_id != main_request_id_ &&
        message_request_id != sub_request_id_) {
      return false;
    }

    IPC_BEGIN_MESSAGE_MAP(Handler, message)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExecuteCodeFinished,
                          OnExecuteCodeFinished)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  // Sends an ExecuteCode message to the given frame host, and increments
  // the number of pending messages.
  void SendExecuteCode(ExtensionMsg_ExecuteCode_Params* params,
                       content::RenderFrameHost* main_frame,
                       content::RenderFrameHost* frame) {
    ++num_pending_;
    params->request_id =
        main_frame == frame ? main_request_id_ : sub_request_id_;
    frame->Send(new ExtensionMsg_ExecuteCode(frame->GetRoutingID(), *params));
  }

  // Handles the ExecuteCodeFinished message.
  void OnExecuteCodeFinished(int request_id,
                             const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& result_list) {
    DCHECK_NE(-1, request_id);
    bool is_main_frame = request_id == main_request_id_;

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
      error_ = error;
      url_ = on_url;
    }

    // Wait until the final request finishes before reporting back.
    if (--num_pending_ > 0)
      return;

    if (script_observers_.get() && error.empty() &&
        host_id_.type() == HostID::EXTENSIONS) {
      ScriptExecutionObserver::ExecutingScriptsMap id_map;
      id_map[host_id_.id()] = std::set<std::string>();
      FOR_EACH_OBSERVER(ScriptExecutionObserver,
                        *script_observers_,
                        OnScriptsExecuted(web_contents(), id_map, on_url));
    }

    Finish(error_, url_, results_);
  }

  void Finish(const std::string& error,
              const GURL& url,
              const base::ListValue& result) {
    if (!callback_.is_null())
      callback_.Run(error, url, result);
    delete this;
  }

  base::WeakPtr<ObserverList<ScriptExecutionObserver>> script_observers_;

  // The id of the host (the extension or the webui) doing the injection.
  HostID host_id_;

  // The request id of the injection into the main frame.
  int main_request_id_;

  // The request id of the injection into any sub frames. We need a separate id
  // for these so that we know which frame to use as the first result, and which
  // error (if any) to use.
  int sub_request_id_;

  // The number of still-running injections.
  int num_pending_;

  // The results of the injection.
  base::ListValue results_;

  // The error from injecting into the main frame.
  std::string error_;

  // The url of the main frame.
  GURL url_;

  // The callback to run after all injections complete.
  ScriptExecutor::ExecuteScriptCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(Handler);
};

}  // namespace

ScriptExecutionObserver::~ScriptExecutionObserver() {
}

ScriptExecutor::ScriptExecutor(
    content::WebContents* web_contents,
    ObserverList<ScriptExecutionObserver>* script_observers)
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
  new Handler(script_observers_, web_contents_, &params, frame_scope,
              &next_request_id_, callback);
}

}  // namespace extensions
