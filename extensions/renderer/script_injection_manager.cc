// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection_manager.h"

#include "base/auto_reset.h"
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
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// The length of time to wait after the DOM is complete to try and run user
// scripts.
const int kScriptIdleTimeoutInMs = 200;

// Returns the RunLocation that follows |run_location|.
UserScript::RunLocation NextRunLocation(UserScript::RunLocation run_location) {
  switch (run_location) {
    case UserScript::DOCUMENT_START:
      return UserScript::DOCUMENT_END;
    case UserScript::DOCUMENT_END:
      return UserScript::DOCUMENT_IDLE;
    case UserScript::DOCUMENT_IDLE:
      return UserScript::RUN_LOCATION_LAST;
    case UserScript::UNDEFINED:
    case UserScript::RUN_DEFERRED:
    case UserScript::BROWSER_DRIVEN:
    case UserScript::RUN_LOCATION_LAST:
      break;
  }
  NOTREACHED();
  return UserScript::RUN_LOCATION_LAST;
}

}  // namespace

class ScriptInjectionManager::RVOHelper : public content::RenderViewObserver {
 public:
  RVOHelper(content::RenderView* render_view, ScriptInjectionManager* manager);
  ~RVOHelper() override;

 private:
  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidCreateNewDocument(blink::WebLocalFrame* frame) override;
  void DidCreateDocumentElement(blink::WebLocalFrame* frame) override;
  void DidFinishDocumentLoad(blink::WebLocalFrame* frame) override;
  void DidFinishLoad(blink::WebLocalFrame* frame) override;
  void FrameDetached(blink::WebFrame* frame) override;
  void OnDestruct() override;

  virtual void OnExecuteCode(const ExtensionMsg_ExecuteCode_Params& params);
  virtual void OnExecuteDeclarativeScript(int tab_id,
                                          const ExtensionId& extension_id,
                                          int script_id,
                                          const GURL& url);
  virtual void OnPermitScriptInjection(int64 request_id);

  // Tells the ScriptInjectionManager to run tasks associated with
  // document_idle.
  void RunIdle(blink::WebFrame* frame);

  // Indicate that the given |frame| is no longer valid because it is starting
  // a new load or closing.
  void InvalidateFrame(blink::WebFrame* frame);

  // The owning ScriptInjectionManager.
  ScriptInjectionManager* manager_;

  // The set of frames that we are about to notify for DOCUMENT_IDLE. We keep
  // a set of those that are valid, so we don't notify that an invalid frame
  // became idle.
  std::set<blink::WebFrame*> pending_idle_frames_;

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
    IPC_MESSAGE_HANDLER(ExtensionMsg_ExecuteDeclarativeScript,
                        OnExecuteDeclarativeScript)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ScriptInjectionManager::RVOHelper::DidCreateNewDocument(
    blink::WebLocalFrame* frame) {
  // A new document is going to be shown, so invalidate the old document state.
  // Check that the frame's state is known before invalidating the frame,
  // because it is possible that a script injection was scheduled before the
  // page was loaded, e.g. by navigating to a javascript: URL before the page
  // has loaded.
  if (manager_->frame_statuses_.find(frame) != manager_->frame_statuses_.end())
    InvalidateFrame(frame);
}

void ScriptInjectionManager::RVOHelper::DidCreateDocumentElement(
    blink::WebLocalFrame* frame) {
  manager_->StartInjectScripts(frame, UserScript::DOCUMENT_START);
}

void ScriptInjectionManager::RVOHelper::DidFinishDocumentLoad(
    blink::WebLocalFrame* frame) {
  manager_->StartInjectScripts(frame, UserScript::DOCUMENT_END);
  pending_idle_frames_.insert(frame);
  // We try to run idle in two places: here and DidFinishLoad.
  // DidFinishDocumentLoad() corresponds to completing the document's load,
  // whereas DidFinishLoad corresponds to completing the document and all
  // subresources' load. We don't want to hold up script injection for a
  // particularly slow subresource, so we set a delayed task from here - but if
  // we finish everything before that point (i.e., DidFinishLoad() is
  // triggered), then there's no reason to keep waiting.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ScriptInjectionManager::RVOHelper::RunIdle,
                 weak_factory_.GetWeakPtr(),
                 frame),
      base::TimeDelta::FromMilliseconds(kScriptIdleTimeoutInMs));
}

void ScriptInjectionManager::RVOHelper::DidFinishLoad(
    blink::WebLocalFrame* frame) {
  // Ensure that we don't block any UI progress by running scripts.
  // We *don't* add the frame to |pending_idle_frames_| here because
  // DidFinishDocumentLoad should strictly come before DidFinishLoad, so the
  // first posted task to RunIdle() pops it out of the set. This ensures we
  // don't try to run idle twice.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ScriptInjectionManager::RVOHelper::RunIdle,
                 weak_factory_.GetWeakPtr(),
                 frame));
}

void ScriptInjectionManager::RVOHelper::FrameDetached(blink::WebFrame* frame) {
  // The frame is closing - invalidate.
  InvalidateFrame(frame);
}

void ScriptInjectionManager::RVOHelper::OnDestruct() {
  manager_->RemoveObserver(this);
}

void ScriptInjectionManager::RVOHelper::OnExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params) {
  manager_->HandleExecuteCode(params, render_view());
}

void ScriptInjectionManager::RVOHelper::OnExecuteDeclarativeScript(
    int tab_id,
    const ExtensionId& extension_id,
    int script_id,
    const GURL& url) {
  blink::WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  CHECK(main_frame);

  // TODO(markdittmer): This would be cleaner if we compared page_ids instead.
  // Begin script injeciton workflow only if the current URL is identical to
  // the one that matched declarative conditions in the browser.
  if (main_frame->top()->document().url() == url) {
    manager_->HandleExecuteDeclarativeScript(main_frame,
                                             tab_id,
                                             extension_id,
                                             script_id,
                                             url);
  }
}

void ScriptInjectionManager::RVOHelper::OnPermitScriptInjection(
    int64 request_id) {
  manager_->HandlePermitScriptInjection(request_id);
}

void ScriptInjectionManager::RVOHelper::RunIdle(blink::WebFrame* frame) {
  // Only notify the manager if the frame hasn't either been removed or already
  // had idle run since the task to RunIdle() was posted.
  if (pending_idle_frames_.count(frame) > 0) {
    manager_->StartInjectScripts(frame, UserScript::DOCUMENT_IDLE);
    pending_idle_frames_.erase(frame);
  }
}

void ScriptInjectionManager::RVOHelper::InvalidateFrame(
    blink::WebFrame* frame) {
  pending_idle_frames_.erase(frame);
  manager_->InvalidateForFrame(frame);
}

ScriptInjectionManager::ScriptInjectionManager(
    const ExtensionSet* extensions,
    UserScriptSetManager* user_script_set_manager)
    : extensions_(extensions),
      injecting_scripts_(false),
      user_script_set_manager_(user_script_set_manager),
      user_script_set_manager_observer_(this) {
  user_script_set_manager_observer_.Add(user_script_set_manager_);
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

bool ScriptInjectionManager::IsFrameValid(blink::WebFrame* frame) const {
  return frame_statuses_.find(frame) != frame_statuses_.end();
}

void ScriptInjectionManager::StartInjectScripts(
    blink::WebFrame* frame, UserScript::RunLocation run_location) {
  FrameStatusMap::iterator iter = frame_statuses_.find(frame);
  // We also don't execute if we detect that the run location is somehow out of
  // order. This can happen if:
  // - The first run location reported for the frame isn't DOCUMENT_START, or
  // - The run location reported doesn't immediately follow the previous
  //   reported run location.
  // We don't want to run because extensions may have requirements that scripts
  // running in an earlier run location have run by the time a later script
  // runs. Better to just not run.
  // Note that we check run_location > NextRunLocation() in the second clause
  // (as opposed to !=) because earlier signals (like DidCreateDocumentElement)
  // can happen multiple times, so we can receive earlier/equal run locations.
  if ((iter == frame_statuses_.end() &&
           run_location != UserScript::DOCUMENT_START) ||
      (iter != frame_statuses_.end() &&
           run_location > NextRunLocation(iter->second))) {
    // We also invalidate the frame, because the run order of pending injections
    // may also be bad.
    InvalidateForFrame(frame);
    return;
  } else if (iter != frame_statuses_.end() && iter->second >= run_location) {
    // Certain run location signals (like DidCreateDocumentElement) can happen
    // multiple times. Ignore the subsequent signals.
    return;
  }

  // Otherwise, all is right in the world, and we can get on with the
  // injections!

  frame_statuses_[frame] = run_location;

  // If a content script injects blocking code (such as a javascript alert()),
  // then there is a chance that we are running in a nested message loop, and
  // shouldn't inject scripts right now (to avoid conflicts).
  if (!injecting_scripts_) {
    InjectScripts(frame, run_location);
    // As above, we might have been blocked, but that means that, in the mean
    // time, it's possible the frame advanced. Inject any scripts for run
    // locations that were registered, but never ran.
    while ((iter = frame_statuses_.find(frame)) != frame_statuses_.end() &&
           iter->second > run_location) {
      run_location = NextRunLocation(run_location);
      DCHECK_LE(run_location, UserScript::DOCUMENT_IDLE);
      InjectScripts(frame, run_location);
    }
  }
}

void ScriptInjectionManager::InjectScripts(
    blink::WebFrame* frame,
    UserScript::RunLocation run_location) {
  DCHECK(!injecting_scripts_);
  base::AutoReset<bool>(&injecting_scripts_, true);
  // Inject any scripts that were waiting for the right run location.
  ScriptsRunInfo scripts_run_info;
  for (ScopedVector<ScriptInjection>::iterator iter =
           pending_injections_.begin();
       iter != pending_injections_.end();) {
    // If a blocking script was injected, there is potentially a possibility
    // that the frame has been invalidated in the time since. Check.
    if (!IsFrameValid(frame))
      return;
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
  user_script_set_manager_->GetAllInjections(
      &user_script_injections, frame, tab_id, run_location);
  for (ScopedVector<ScriptInjection>::iterator iter =
           user_script_injections.begin();
       iter != user_script_injections.end();) {
    // If a blocking script was injected, there is potentially a possibility
    // that the frame has been invalidated in the time since. Check.
    if (!IsFrameValid(frame))
      return;
    scoped_ptr<ScriptInjection> injection(*iter);
    iter = user_script_injections.weak_erase(iter);
    if (!injection->TryToInject(run_location,
                                extensions_->GetByID(injection->extension_id()),
                                &scripts_run_info)) {
      pending_injections_.push_back(injection.release());
    }
  }

  if (IsFrameValid(frame))
    scripts_run_info.LogRun(frame, run_location);
}

void ScriptInjectionManager::HandleExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params,
    content::RenderView* render_view) {
  // TODO(dcheng): Not sure how this can happen today. In an OOPI world, it
  // would indicate a logic error--the browser must direct this request to the
  // right renderer process to begin with.
  blink::WebLocalFrame* main_frame =
      render_view->GetWebView()->mainFrame()->toWebLocalFrame();
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

void ScriptInjectionManager::HandleExecuteDeclarativeScript(
    blink::WebFrame* web_frame,
    int tab_id,
    const ExtensionId& extension_id,
    int script_id,
    const GURL& url) {
  const Extension* extension = extensions_->GetByID(extension_id);
  // TODO(dcheng): This function signature should really be a WebLocalFrame,
  // rather than trying to coerce it here.
  scoped_ptr<ScriptInjection> injection =
      user_script_set_manager_->GetInjectionForDeclarativeScript(
          script_id,
          web_frame->toWebLocalFrame(),
          tab_id,
          url,
          extension);
  if (injection.get()) {
    ScriptsRunInfo scripts_run_info;
    // TODO(markdittmer): Use return value of TryToInject for error handling.
    injection->TryToInject(UserScript::BROWSER_DRIVEN,
                           extension,
                           &scripts_run_info);
    scripts_run_info.LogRun(web_frame, UserScript::BROWSER_DRIVEN);
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
