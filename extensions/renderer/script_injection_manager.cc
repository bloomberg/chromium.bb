// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection_manager.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/programmatic_script_injector.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/scripts_run_info.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// The length of time to wait after the DOM is complete to try and run user
// scripts.
const int kScriptIdleTimeoutInMs = 200;

}  // namespace

class ScriptInjectionManager::RVOHelper : public content::RenderViewObserver {
 public:
  RVOHelper(content::RenderView* render_view, ScriptInjectionManager* manager);
  virtual ~RVOHelper();

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCreateDocumentElement(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidFinishDocumentLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidStartProvisionalLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void FrameDetached(blink::WebFrame* frame) OVERRIDE;
  virtual void OnDestruct() OVERRIDE;

  virtual void OnExecuteCode(const ExtensionMsg_ExecuteCode_Params& params);
  virtual void OnPermitScriptInjection(int64 request_id);

  // Tells the ScriptInjectionManager to run tasks associated with
  // document_idle.
  void RunIdle(blink::WebFrame* frame);

  ScriptInjectionManager* manager_;
  base::WeakPtrFactory<RVOHelper> weak_factory_;
};

ScriptInjectionManager::RVOHelper::RVOHelper(
    content::RenderView* render_view,
    ScriptInjectionManager* manager)
    : content::RenderViewObserver(render_view),
      manager_(manager),
      weak_factory_(this) {
}

ScriptInjectionManager::RVOHelper::~RVOHelper() {
}

bool ScriptInjectionManager::RVOHelper::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ScriptInjectionManager::RVOHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ExecuteCode, OnExecuteCode)
    IPC_MESSAGE_HANDLER(ExtensionMsg_PermitScriptInjection,
                        OnPermitScriptInjection)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ScriptInjectionManager::RVOHelper::DidCreateDocumentElement(
    blink::WebLocalFrame* frame) {
  manager_->InjectScripts(frame, UserScript::DOCUMENT_START);
}

void ScriptInjectionManager::RVOHelper::DidFinishDocumentLoad(
    blink::WebLocalFrame* frame) {
  manager_->InjectScripts(frame, UserScript::DOCUMENT_END);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ScriptInjectionManager::RVOHelper::RunIdle,
                 weak_factory_.GetWeakPtr(),
                 frame),
      base::TimeDelta::FromMilliseconds(kScriptIdleTimeoutInMs));
}

void ScriptInjectionManager::RVOHelper::DidFinishLoad(
    blink::WebLocalFrame* frame) {
  // Ensure that running scripts does not keep any progress UI running.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ScriptInjectionManager::RVOHelper::RunIdle,
                 weak_factory_.GetWeakPtr(),
                 frame));
}

void ScriptInjectionManager::RVOHelper::DidStartProvisionalLoad(
    blink::WebLocalFrame* frame) {
  manager_->InvalidateForFrame(frame);
}

void ScriptInjectionManager::RVOHelper::FrameDetached(blink::WebFrame* frame) {
  manager_->InvalidateForFrame(frame);
}

void ScriptInjectionManager::RVOHelper::OnDestruct() {
  manager_->RemoveObserver(this);
}

void ScriptInjectionManager::RVOHelper::OnExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params) {
  manager_->HandleExecuteCode(params, render_view());
}

void ScriptInjectionManager::RVOHelper::OnPermitScriptInjection(
    int64 request_id) {
  manager_->HandlePermitScriptInjection(request_id);
}

void ScriptInjectionManager::RVOHelper::RunIdle(blink::WebFrame* frame) {
  manager_->InjectScripts(frame, UserScript::DOCUMENT_IDLE);
}

ScriptInjectionManager::ScriptInjectionManager(
    const ExtensionSet* extensions,
    UserScriptSet* user_script_set)
    : extensions_(extensions),
      user_script_set_(user_script_set),
      user_script_set_observer_(this) {
  user_script_set_observer_.Add(user_script_set_);
}

ScriptInjectionManager::~ScriptInjectionManager() {
}

void ScriptInjectionManager::OnRenderViewCreated(
    content::RenderView* render_view) {
  rvo_helpers_.push_back(new RVOHelper(render_view, this));
}

void ScriptInjectionManager::OnUserScriptsUpdated(
    const std::set<std::string>& changed_extensions,
    const std::vector<UserScript*>& scripts) {
  for (ScopedVector<ScriptInjection>::iterator iter =
           pending_injections_.begin();
       iter != pending_injections_.end();) {
    if (changed_extensions.count((*iter)->extension_id()) > 0)
      iter = pending_injections_.erase(iter);
    else
      ++iter;
  }
}

void ScriptInjectionManager::RemoveObserver(RVOHelper* helper) {
  for (ScopedVector<RVOHelper>::iterator iter = rvo_helpers_.begin();
       iter != rvo_helpers_.end();
       ++iter) {
    if (*iter == helper) {
      rvo_helpers_.erase(iter);
      break;
    }
  }
}

void ScriptInjectionManager::InvalidateForFrame(blink::WebFrame* frame) {
  for (ScopedVector<ScriptInjection>::iterator iter =
           pending_injections_.begin();
       iter != pending_injections_.end();) {
    if ((*iter)->web_frame() == frame)
      iter = pending_injections_.erase(iter);
    else
      ++iter;
  }

  frame_statuses_.erase(frame);
}

void ScriptInjectionManager::InjectScripts(
    blink::WebFrame* frame, UserScript::RunLocation run_location) {
  FrameStatusMap::iterator iter = frame_statuses_.find(frame);
  // Check if the frame is already in our map.
  if (iter == frame_statuses_.end()) {
    // If the frame isn't in our map, and the run location isn't document_start,
    // then abort. This can happen in two ways:
    // 1. We just received a delayed idle run for a frame which is invalidated.
    //    Obviously, we don't want to run.
    // 2. We somehow received a document_end or document_idle notification
    //    without ever receiving a document_start. We don't want to run because
    //    extensions may have requirements that scripts running at start have
    //    run by the time scripts run at idle. Better to just not run.
    if (run_location != UserScript::DOCUMENT_START)
      return;

    // Otherwise, add a new entry to the map.
    frame_statuses_[frame] = UserScript::DOCUMENT_START;
  } else {  // Already in the map.
    // If we've already run the given location (happens in the case of idle
    // since we notify in two places), return.
    if (iter->second == run_location)
      return;

    // Otherwise, update the frame status.
    iter->second = run_location;
  }

  // Inject any scripts that were waiting for the right run location.
  ScriptsRunInfo scripts_run_info;
  for (ScopedVector<ScriptInjection>::iterator iter =
           pending_injections_.begin();
       iter != pending_injections_.end();) {
    if ((*iter)->web_frame() == frame &&
        (*iter)->TryToInject(run_location,
                             extensions_->GetByID((*iter)->extension_id()),
                             &scripts_run_info)) {
      iter = pending_injections_.erase(iter);
    } else {
      ++iter;
    }
  }

  // Try to inject any user scripts that should run for this location. If they
  // don't complete their injection (for example, waiting for a permission
  // response) then they will be added to |pending_injections_|.
  ScopedVector<ScriptInjection> user_script_injections;
  int tab_id = ExtensionHelper::Get(content::RenderView::FromWebView(
                                        frame->top()->view()))->tab_id();
  user_script_set_->GetInjections(
      &user_script_injections, frame, tab_id, run_location);
  for (ScopedVector<ScriptInjection>::iterator iter =
           user_script_injections.begin();
       iter != user_script_injections.end();) {
    scoped_ptr<ScriptInjection> injection(*iter);
    iter = user_script_injections.weak_erase(iter);
    if (!injection->TryToInject(run_location,
                                extensions_->GetByID(injection->extension_id()),
                                &scripts_run_info)) {
      pending_injections_.push_back(injection.release());
    }
  }

  scripts_run_info.LogRun(frame, run_location);
}

void ScriptInjectionManager::HandleExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params,
    content::RenderView* render_view) {
  blink::WebFrame* main_frame = render_view->GetWebView()->mainFrame();
  if (!main_frame) {
    render_view->Send(
        new ExtensionHostMsg_ExecuteCodeFinished(render_view->GetRoutingID(),
                                                 params.request_id,
                                                 "No main frame",
                                                 GURL(std::string()),
                                                 base::ListValue()));
    return;
  }

  scoped_ptr<ScriptInjection> injection(new ScriptInjection(
      scoped_ptr<ScriptInjector>(
          new ProgrammaticScriptInjector(params, main_frame)),
      main_frame,
      params.extension_id,
      static_cast<UserScript::RunLocation>(params.run_at),
      ExtensionHelper::Get(render_view)->tab_id()));

  ScriptsRunInfo scripts_run_info;
  FrameStatusMap::const_iterator iter = frame_statuses_.find(main_frame);
  if (!injection->TryToInject(
          iter == frame_statuses_.end() ? UserScript::UNDEFINED : iter->second,
          extensions_->GetByID(injection->extension_id()),
          &scripts_run_info)) {
    pending_injections_.push_back(injection.release());
  }
}

void ScriptInjectionManager::HandlePermitScriptInjection(int64 request_id) {
  ScopedVector<ScriptInjection>::iterator iter =
      pending_injections_.begin();
  for (; iter != pending_injections_.end(); ++iter) {
    if ((*iter)->request_id() == request_id)
      break;
  }
  if (iter == pending_injections_.end())
    return;

  // At this point, because the request is present in pending_injections_, we
  // know that this is the same page that issued the request (otherwise,
  // RVOHelper's DidStartProvisionalLoad callback would have caused it to be
  // cleared out).

  scoped_ptr<ScriptInjection> injection(*iter);
  pending_injections_.weak_erase(iter);

  ScriptsRunInfo scripts_run_info;
  if (injection->OnPermissionGranted(extensions_->GetByID(
                                         injection->extension_id()),
                                     &scripts_run_info)) {
    scripts_run_info.LogRun(injection->web_frame(), UserScript::RUN_DEFERRED);
  }
}

}  // namespace extensions
