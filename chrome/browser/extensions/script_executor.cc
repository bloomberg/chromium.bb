// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_executor.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
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
  Handler(ObserverList<TabHelper::ScriptExecutionObserver>* script_observers,
          content::WebContents* web_contents,
          const ExtensionMsg_ExecuteCode_Params& params,
          const ScriptExecutor::ExecuteScriptCallback& callback)
          : content::WebContentsObserver(web_contents),
            script_observers_(AsWeakPtr(script_observers)),
            extension_id_(params.extension_id),
            request_id_(params.request_id),
            callback_(callback) {
    content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
    rvh->Send(new ExtensionMsg_ExecuteCode(rvh->GetRoutingID(), params));
  }

  virtual ~Handler() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    // Unpack by hand to check the request_id, since there may be multiple
    // requests in flight but only one is for this.
    if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
      return false;

    int message_request_id;
    PickleIterator iter(message);
    CHECK(message.ReadInt(&iter, &message_request_id));

    if (message_request_id != request_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(Handler, message)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExecuteCodeFinished,
                          OnExecuteCodeFinished)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  virtual void WebContentsDestroyed(content::WebContents* tab) OVERRIDE {
    base::ListValue val;
    callback_.Run(kRendererDestroyed, -1, GURL(""), val);
    delete this;
  }

 private:
  void OnExecuteCodeFinished(int request_id,
                             const std::string& error,
                             int32 on_page_id,
                             const GURL& on_url,
                             const base::ListValue& script_result) {
    if (script_observers_ && error.empty()) {
      TabHelper::ScriptExecutionObserver::ExecutingScriptsMap id_map;
      id_map[extension_id_] = std::set<std::string>();
      FOR_EACH_OBSERVER(TabHelper::ScriptExecutionObserver, *script_observers_,
                        OnScriptsExecuted(web_contents(),
                                          id_map,
                                          on_page_id,
                                          on_url));
    }

    callback_.Run(error, on_page_id, on_url, script_result);
    delete this;
  }

  base::WeakPtr<ObserverList<TabHelper::ScriptExecutionObserver> >
      script_observers_;
  std::string extension_id_;
  int request_id_;
  ScriptExecutor::ExecuteScriptCallback callback_;
};

}  // namespace

ScriptExecutor::ScriptExecutor(
    content::WebContents* web_contents,
    ObserverList<TabHelper::ScriptExecutionObserver>* script_observers)
    : next_request_id_(0),
      web_contents_(web_contents),
      script_observers_(script_observers) {}

ScriptExecutor::~ScriptExecutor() {}

void ScriptExecutor::ExecuteScript(
    const std::string& extension_id,
    ScriptExecutor::ScriptType script_type,
    const std::string& code,
    ScriptExecutor::FrameScope frame_scope,
    UserScript::RunLocation run_at,
    ScriptExecutor::WorldType world_type,
    bool is_web_view,
    const ExecuteScriptCallback& callback) {
  ExtensionMsg_ExecuteCode_Params params;
  params.request_id = next_request_id_++;
  params.extension_id = extension_id;
  params.is_javascript = (script_type == JAVASCRIPT);
  params.code = code;
  params.all_frames = (frame_scope == ALL_FRAMES);
  params.run_at = static_cast<int>(run_at);
  params.in_main_world = (world_type == MAIN_WORLD);
  params.is_web_view = is_web_view;

  // Handler handles IPCs and deletes itself on completion.
  new Handler(script_observers_, web_contents_, params, callback);
}

}  // namespace extensions
