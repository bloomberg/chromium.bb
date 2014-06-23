// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/programmatic_script_injection.h"

#include <vector>

#include "base/values.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Append all the child frames of |parent_frame| to |frames_vector|.
void AppendAllChildFrames(blink::WebFrame* parent_frame,
                          std::vector<blink::WebFrame*>* frames_vector) {
  DCHECK(parent_frame);
  for (blink::WebFrame* child_frame = parent_frame->firstChild();
       child_frame;
       child_frame = child_frame->nextSibling()) {
    frames_vector->push_back(child_frame);
    AppendAllChildFrames(child_frame, frames_vector);
  }
}

}  // namespace

ProgrammaticScriptInjection::ProgrammaticScriptInjection(
    blink::WebFrame* web_frame,
    const ExtensionMsg_ExecuteCode_Params& params,
    int tab_id)
    : ScriptInjection(web_frame,
                      params.extension_id,
                      static_cast<UserScript::RunLocation>(params.run_at),
                      tab_id),
      params_(new ExtensionMsg_ExecuteCode_Params(params)) {
}

ProgrammaticScriptInjection::~ProgrammaticScriptInjection() {
}

bool ProgrammaticScriptInjection::TryToInject(
    UserScript::RunLocation current_location,
    const Extension* extension,
    ScriptsRunInfo* scripts_run_info) {
  if (current_location < run_location())
    return false;  // Wait for the right location.

  if (!extension) {
    content::RenderView* render_view = GetRenderView();
    // Inform the browser process that we are not executing the script.
    render_view->Send(
        new ExtensionHostMsg_ExecuteCodeFinished(render_view->GetRoutingID(),
                                                 params_->request_id,
                                                 std::string(),  // no error
                                                 -1,             // no page id
                                                 GURL(std::string()),
                                                 base::ListValue()));
    return true;  // We're done.
  }

  GURL url = web_frame()->document().url();
  bool can_execute = CanExecuteOnFrame(extension, web_frame(), url);
  if (!can_execute) {
    // If we can't execute on the frame, send an error message and abort.
    content::RenderView* render_view = GetRenderView();
    render_view->Send(new ExtensionHostMsg_ExecuteCodeFinished(
        render_view->GetRoutingID(),
        params_->request_id,
        ErrorUtils::FormatErrorMessage(manifest_errors::kCannotAccessPage,
                                       url.spec()),
        render_view->GetPageId(),
        ScriptContext::GetDataSourceURLForFrame(web_frame()),
        base::ListValue()));
     return true;  // We're done.
  }

  // Otherwise, we're good to go.
  Inject(extension, scripts_run_info);
  return true;  // We're done.
}

bool ProgrammaticScriptInjection::OnPermissionGranted(
    const Extension* extension,
    ScriptsRunInfo* scripts_run_info) {
  // ProgrammaticScriptInjections shouldn't get permission via IPC (yet).
  NOTREACHED();
  return false;
}

content::RenderView* ProgrammaticScriptInjection::GetRenderView() {
  return content::RenderView::FromWebView(web_frame()->view());
}

bool ProgrammaticScriptInjection::CanExecuteOnFrame(
    const Extension* extension,
    const blink::WebFrame* frame,
    const GURL& top_url) const {
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->document().url(), params_->match_about_blank);
  if (params_->is_web_view)
    return effective_document_url == params_->webview_src;

  return extension->permissions_data()->CanAccessPage(extension,
                                                      effective_document_url,
                                                      top_url,
                                                      tab_id(),
                                                      -1,     // no process ID.
                                                      NULL);  // ignore error.
}

void ProgrammaticScriptInjection::Inject(const Extension* extension,
                                         ScriptsRunInfo* scripts_run_info) {
  std::vector<blink::WebFrame*> frame_vector;
  frame_vector.push_back(web_frame());
  if (params_->all_frames)
    AppendAllChildFrames(web_frame(), &frame_vector);

  scoped_ptr<blink::WebScopedUserGesture> gesture;
  if (params_->user_gesture)
    gesture.reset(new blink::WebScopedUserGesture);

  GURL top_url = web_frame()->document().url();
  base::ListValue execution_results;
  for (std::vector<blink::WebFrame*>::iterator frame_it = frame_vector.begin();
       frame_it != frame_vector.end();
       ++frame_it) {
    blink::WebFrame* child_frame = *frame_it;
    CHECK(child_frame) << top_url;

    // We recheck access here in the renderer for extra safety against races
    // with navigation, but different frames can have different URLs, and the
    // extension might only have access to a subset of them.
    // For child frames, we just skip ones the extension doesn't have access
    // to and carry on.
    if (!CanExecuteOnFrame(extension, child_frame, top_url)) {
      // If we couldn't access the top frame, we shouldn't be injecting at all.
      DCHECK(child_frame->parent());
      continue;
    }

    if (params_->is_javascript) {
      blink::WebScriptSource source(blink::WebString::fromUTF8(params_->code),
                                    params_->file_url);
      v8::HandleScope scope(v8::Isolate::GetCurrent());

      scoped_ptr<content::V8ValueConverter> v8_converter(
          content::V8ValueConverter::create());
      v8::Local<v8::Value> script_value;

      if (params_->in_main_world) {
        DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId,
                                         extension->id());
        script_value = child_frame->executeScriptAndReturnValue(source);
      } else {  // in isolated world
        blink::WebVector<v8::Local<v8::Value> > results;
        std::vector<blink::WebScriptSource> sources;
        sources.push_back(source);
        int isolated_world_id =
            GetIsolatedWorldIdForExtension(extension, child_frame);
        DOMActivityLogger::AttachToWorld(isolated_world_id, extension->id());
        child_frame->executeScriptInIsolatedWorld(
            isolated_world_id,
            &sources.front(),
            sources.size(),
            EXTENSION_GROUP_CONTENT_SCRIPTS,
            &results);
        // We only expect one value back since we only pushed one source
        if (results.size() == 1 && !results[0].IsEmpty())
          script_value = results[0];
      }

      if (params_->wants_result && !script_value.IsEmpty()) {
        // It's safe to always use the main world context when converting here.
        // V8ValueConverterImpl shouldn't actually care about the context scope,
        // and it switches to v8::Object's creation context when encountered.
        v8::Local<v8::Context> context = child_frame->mainWorldScriptContext();
        base::Value* result = v8_converter->FromV8Value(script_value, context);
        // Always append an execution result (i.e. no result == null result) so
        // that |execution_results| lines up with the frames.
        execution_results.Append(result ? result
                                        : base::Value::CreateNullValue());
      }
    } else {  // css
      child_frame->document().insertStyleSheet(
          blink::WebString::fromUTF8(params_->code));
    }
  }

  content::RenderView* render_view = GetRenderView();
  render_view->Send(new ExtensionHostMsg_ExecuteCodeFinished(
      render_view->GetRoutingID(),
      params_->request_id,
      std::string(),  // no error.
      render_view->GetPageId(),
      ScriptContext::GetDataSourceURLForFrame(web_frame()),
      execution_results));
}

}  // namespace extensions
