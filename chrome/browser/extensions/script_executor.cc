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

namespace extensions {

namespace {

const char* kRendererDestroyed = "The tab was closed.";

// A handler for a single injection request. On creation this will send the
// injection request to the renderer, and it will be destroyed after either the
// corresponding response comes from the renderer, or the renderer is destroyed.
class Handler : public content::WebContentsObserver {
 public:
  Handler(ObserverList<ScriptExecutor::Observer>* observer_list,
          content::WebContents* web_contents,
          const ExtensionMsg_ExecuteCode_Params& params,
          const ScriptExecutor::ExecuteScriptCallback& callback)
          : content::WebContentsObserver(web_contents),
            observer_list_(AsWeakPtr(observer_list)),
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
    callback_.Run(false, -1, kRendererDestroyed);
    delete this;
  }

 private:
  void OnExecuteCodeFinished(int request_id,
                             bool success,
                             int32 page_id,
                             const std::string& error) {
    if (observer_list_) {
      FOR_EACH_OBSERVER(ScriptExecutor::Observer, *observer_list_,
                        OnExecuteScriptFinished(extension_id_, success,
                                                page_id, error));
    }

    callback_.Run(success, page_id, error);
    delete this;
  }

  base::WeakPtr<ObserverList<ScriptExecutor::Observer> > observer_list_;
  std::string extension_id_;
  int request_id_;
  ScriptExecutor::ExecuteScriptCallback callback_;
};

}  // namespace

ScriptExecutor::Observer::Observer(ScriptExecutor* script_executor)
    : script_executor_(*script_executor){
  script_executor_.AddObserver(this);
}

ScriptExecutor::Observer::~Observer() {
  script_executor_.RemoveObserver(this);
}

ScriptExecutor::ScriptExecutor(content::WebContents* web_contents)
    : next_request_id_(0),
      web_contents_(web_contents) {}

ScriptExecutor::~ScriptExecutor() {}

void ScriptExecutor::ExecuteScript(
    const std::string& extension_id,
    ScriptExecutor::ScriptType script_type,
    const std::string& code,
    ScriptExecutor::FrameScope frame_scope,
    UserScript::RunLocation run_at,
    ScriptExecutor::WorldType world_type,
    const ExecuteScriptCallback& callback) {
  ExtensionMsg_ExecuteCode_Params params;
  params.request_id = next_request_id_++;
  params.extension_id = extension_id;
  params.is_javascript = (script_type == JAVASCRIPT);
  params.code = code;
  params.all_frames = (frame_scope == ALL_FRAMES);
  params.run_at = (int) run_at;
  params.in_main_world = (world_type == MAIN_WORLD);

  // Handler handles IPCs and deletes itself on completion.
  new Handler(&observer_list_, web_contents_, params, callback);
}

}  // namespace extensions
