// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/extension_injection_host.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/script_injection_callback.h"
#include "extensions/renderer/script_injection_manager.h"
#include "extensions/renderer/scripts_run_info.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using IsolatedWorldMap = std::map<std::string, int>;
base::LazyInstance<IsolatedWorldMap> g_isolated_worlds =
    LAZY_INSTANCE_INITIALIZER;

const int64 kInvalidRequestId = -1;

// The id of the next pending injection.
int64 g_next_pending_id = 0;

// Append all the child frames of |parent_frame| to |frames_vector|.
void AppendAllChildFrames(blink::WebFrame* parent_frame,
                          std::vector<blink::WebFrame*>* frames_vector) {
  DCHECK(parent_frame);
  for (blink::WebFrame* child_frame = parent_frame->firstChild(); child_frame;
       child_frame = child_frame->nextSibling()) {
    frames_vector->push_back(child_frame);
    AppendAllChildFrames(child_frame, frames_vector);
  }
}

// Gets the isolated world ID to use for the given |injection_host|
// in the given |frame|. If no isolated world has been created for that
// |injection_host| one will be created and initialized.
int GetIsolatedWorldIdForInstance(const InjectionHost* injection_host,
                                  blink::WebLocalFrame* frame) {
  static int g_next_isolated_world_id =
      ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();

  IsolatedWorldMap& isolated_worlds = g_isolated_worlds.Get();

  int id = 0;
  const std::string& key = injection_host->id().id();
  IsolatedWorldMap::iterator iter = isolated_worlds.find(key);
  if (iter != isolated_worlds.end()) {
    id = iter->second;
  } else {
    id = g_next_isolated_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough injection hosts for it to matter.
    isolated_worlds[key] = id;
  }

  // We need to set the isolated world origin and CSP even if it's not a new
  // world since these are stored per frame, and we might not have used this
  // isolated world in this frame before.
  frame->setIsolatedWorldSecurityOrigin(
      id, blink::WebSecurityOrigin::create(injection_host->url()));
  frame->setIsolatedWorldContentSecurityPolicy(
      id, blink::WebString::fromUTF8(
          injection_host->GetContentSecurityPolicy()));
  frame->setIsolatedWorldHumanReadableName(
      id, blink::WebString::fromUTF8(injection_host->name()));

  return id;
}

}  // namespace

// static
std::string ScriptInjection::GetHostIdForIsolatedWorld(int isolated_world_id) {
  const IsolatedWorldMap& isolated_worlds = g_isolated_worlds.Get();

  for (const auto& iter : isolated_worlds) {
    if (iter.second == isolated_world_id)
      return iter.first;
  }
  return std::string();
}

// static
void ScriptInjection::RemoveIsolatedWorld(const std::string& host_id) {
  g_isolated_worlds.Get().erase(host_id);
}

ScriptInjection::ScriptInjection(
    scoped_ptr<ScriptInjector> injector,
    blink::WebLocalFrame* web_frame,
    scoped_ptr<const InjectionHost> injection_host,
    UserScript::RunLocation run_location,
    int tab_id)
    : injector_(injector.Pass()),
      web_frame_(web_frame),
      injection_host_(injection_host.Pass()),
      run_location_(run_location),
      tab_id_(tab_id),
      request_id_(kInvalidRequestId),
      complete_(false),
      running_frames_(0),
      execution_results_(new base::ListValue()),
      all_injections_started_(false),
      script_injection_manager_(nullptr) {
  CHECK(injection_host_.get());
}

ScriptInjection::~ScriptInjection() {
  if (!complete_)
    injector_->OnWillNotInject(ScriptInjector::WONT_INJECT);
}

ScriptInjection::InjectionResult ScriptInjection::TryToInject(
    UserScript::RunLocation current_location,
    ScriptsRunInfo* scripts_run_info,
    ScriptInjectionManager* manager) {
  if (current_location < run_location_)
    return INJECTION_WAITING;  // Wait for the right location.

  if (request_id_ != kInvalidRequestId) {
    // We're waiting for permission right now, try again later.
    return INJECTION_WAITING;
  }

  if (!injection_host_) {
    NotifyWillNotInject(ScriptInjector::EXTENSION_REMOVED);
    return INJECTION_FINISHED;  // We're done.
  }

  switch (injector_->CanExecuteOnFrame(
      injection_host_.get(), web_frame_, tab_id_,
      web_frame_->top()->document().url())) {
    case PermissionsData::ACCESS_DENIED:
      NotifyWillNotInject(ScriptInjector::NOT_ALLOWED);
      return INJECTION_FINISHED;  // We're done.
    case PermissionsData::ACCESS_WITHHELD:
      SendInjectionMessage(true /* request permission */);
      return INJECTION_WAITING;  // Wait around for permission.
    case PermissionsData::ACCESS_ALLOWED:
      InjectionResult result = Inject(scripts_run_info);
      // If the injection is blocked, we need to set the manager so we can
      // notify it upon completion.
      if (result == INJECTION_BLOCKED)
        script_injection_manager_ = manager;
      return result;
  }

  NOTREACHED();
  return INJECTION_FINISHED;
}

ScriptInjection::InjectionResult ScriptInjection::OnPermissionGranted(
    ScriptsRunInfo* scripts_run_info) {
  if (!injection_host_) {
    NotifyWillNotInject(ScriptInjector::EXTENSION_REMOVED);
    return INJECTION_FINISHED;
  }

  return Inject(scripts_run_info);
}

void ScriptInjection::OnHostRemoved() {
  injection_host_.reset(nullptr);
}

void ScriptInjection::SendInjectionMessage(bool request_permission) {
  content::RenderView* render_view =
      content::RenderView::FromWebView(web_frame()->top()->view());

  // If we are just notifying the browser of the injection, then send an
  // invalid request (which is treated like a notification).
  request_id_ = request_permission ? g_next_pending_id++ : kInvalidRequestId;
  render_view->Send(new ExtensionHostMsg_RequestScriptInjectionPermission(
      render_view->GetRoutingID(),
      host_id().id(),
      injector_->script_type(),
      request_id_));
}

void ScriptInjection::NotifyWillNotInject(
    ScriptInjector::InjectFailureReason reason) {
  complete_ = true;
  injector_->OnWillNotInject(reason);
}

ScriptInjection::InjectionResult ScriptInjection::Inject(
    ScriptsRunInfo* scripts_run_info) {
  DCHECK(injection_host_);
  DCHECK(scripts_run_info);
  DCHECK(!complete_);

  if (injection_host_->ShouldNotifyBrowserOfInjection())
    SendInjectionMessage(false /* don't request permission */);

  std::vector<blink::WebFrame*> frame_vector;
  frame_vector.push_back(web_frame_);
  if (injector_->ShouldExecuteInChildFrames())
    AppendAllChildFrames(web_frame_, &frame_vector);

  bool inject_js = injector_->ShouldInjectJs(run_location_);
  bool inject_css = injector_->ShouldInjectCss(run_location_);
  DCHECK(inject_js || inject_css);

  GURL top_url = web_frame_->top()->document().url();
  for (std::vector<blink::WebFrame*>::iterator iter = frame_vector.begin();
       iter != frame_vector.end();
       ++iter) {
    // TODO(dcheng): Unfortunately, the code as written won't work in an OOPI
    // world. This is just a temporary hack to make things compile.
    blink::WebLocalFrame* frame = (*iter)->toWebLocalFrame();

    // We recheck access here in the renderer for extra safety against races
    // with navigation, but different frames can have different URLs, and the
    // injection host might only have access to a subset of them.
    // For child frames, we just skip ones the injection host doesn't have
    // access to and carry on.
    // Note: we don't consider ACCESS_WITHHELD because there is nowhere to
    // surface a request for a child frame.
    // TODO(rdevlin.cronin): We should ask for permission somehow.
    if (injector_->CanExecuteOnFrame(
        injection_host_.get(), frame, tab_id_, top_url) ==
            PermissionsData::ACCESS_DENIED) {
      DCHECK(frame->parent());
      continue;
    }
    if (inject_js)
      InjectJs(frame);
    if (inject_css)
      InjectCss(frame);
  }

  all_injections_started_ = true;
  injector_->GetRunInfo(scripts_run_info, run_location_);
  scripts_run_info->num_blocking_js = running_frames_;
  TryToFinish();
  return complete_ ? INJECTION_FINISHED : INJECTION_BLOCKED;
}

void ScriptInjection::InjectJs(blink::WebLocalFrame* frame) {
  ++running_frames_;
  std::vector<blink::WebScriptSource> sources =
      injector_->GetJsSources(run_location_);
  bool in_main_world = injector_->ShouldExecuteInMainWorld();
  int world_id = in_main_world
                     ? DOMActivityLogger::kMainWorldId
                     : GetIsolatedWorldIdForInstance(injection_host_.get(),
                                                     frame);
  bool is_user_gesture = injector_->IsUserGesture();

  scoped_ptr<blink::WebScriptExecutionCallback> callback(
      new ScriptInjectionCallback(this, frame));

  base::ElapsedTimer exec_timer;
  if (injection_host_->id().type() == HostID::EXTENSIONS)
    DOMActivityLogger::AttachToWorld(world_id, injection_host_->id().id());
  if (in_main_world) {
    // We only inject in the main world for javascript: urls.
    DCHECK_EQ(1u, sources.size());

    frame->requestExecuteScriptAndReturnValue(sources.front(),
                                              is_user_gesture,
                                              callback.release());
  } else {
    frame->requestExecuteScriptInIsolatedWorld(world_id,
                                               &sources.front(),
                                               sources.size(),
                                               EXTENSION_GROUP_CONTENT_SCRIPTS,
                                               is_user_gesture,
                                               callback.release());
  }

  if (injection_host_->id().type() == HostID::EXTENSIONS)
    UMA_HISTOGRAM_TIMES("Extensions.InjectScriptTime", exec_timer.Elapsed());
}

void ScriptInjection::OnJsInjectionCompleted(
    blink::WebLocalFrame* frame,
    const blink::WebVector<v8::Local<v8::Value> >& results) {
  DCHECK(running_frames_ > 0);
  --running_frames_;

  bool expects_results = injector_->ExpectsResults();
  if (expects_results) {
    v8::Local<v8::Value> script_value;
    if (!results.isEmpty())
      script_value = results[0];
    // Right now, we only support returning single results (per frame).
    scoped_ptr<content::V8ValueConverter> v8_converter(
        content::V8ValueConverter::create());
    // It's safe to always use the main world context when converting
    // here. V8ValueConverterImpl shouldn't actually care about the
    // context scope, and it switches to v8::Object's creation context
    // when encountered.
    v8::Local<v8::Context> context = frame->mainWorldScriptContext();
    scoped_ptr<base::Value> result(
        v8_converter->FromV8Value(script_value, context));
    if (!result.get())
      result.reset(base::Value::CreateNullValue());
    // We guarantee that the main frame's result is at the first index, but
    // any sub frames results do not have guaranteed order.
    execution_results_->Insert(
        frame == web_frame_ ? 0 : execution_results_->GetSize(),
        result.release());
  }
  TryToFinish();
}

void ScriptInjection::TryToFinish() {
  if (all_injections_started_ && running_frames_ == 0) {
    complete_ = true;
    injector_->OnInjectionComplete(execution_results_.Pass(),
                                   run_location_);

    // This object can be destroyed after next line.
    if (script_injection_manager_)
      script_injection_manager_->OnInjectionFinished(this);
  }
}

void ScriptInjection::InjectCss(blink::WebLocalFrame* frame) {
  std::vector<std::string> css_sources =
      injector_->GetCssSources(run_location_);
  for (std::vector<std::string>::const_iterator iter = css_sources.begin();
       iter != css_sources.end();
       ++iter) {
    frame->document().insertStyleSheet(blink::WebString::fromUTF8(*iter));
  }
}

}  // namespace extensions
