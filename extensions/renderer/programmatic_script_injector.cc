// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/programmatic_script_injector.h"

#include <vector>

#include "base/values.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

namespace extensions {

ProgrammaticScriptInjector::ProgrammaticScriptInjector(
    const ExtensionMsg_ExecuteCode_Params& params,
    blink::WebFrame* web_frame)
    : params_(new ExtensionMsg_ExecuteCode_Params(params)),
      url_(ScriptContext::GetDataSourceURLForFrame(web_frame)),
      render_view_(content::RenderView::FromWebView(web_frame->view())),
      results_(new base::ListValue()),
      finished_(false) {
}

ProgrammaticScriptInjector::~ProgrammaticScriptInjector() {
}

UserScript::InjectionType ProgrammaticScriptInjector::script_type()
    const {
  return UserScript::PROGRAMMATIC_SCRIPT;
}

bool ProgrammaticScriptInjector::ShouldExecuteInChildFrames() const {
  return params_->all_frames;
}

bool ProgrammaticScriptInjector::ShouldExecuteInMainWorld() const {
  return params_->in_main_world;
}

bool ProgrammaticScriptInjector::IsUserGesture() const {
  return params_->user_gesture;
}

bool ProgrammaticScriptInjector::ExpectsResults() const {
  return params_->wants_result;
}

bool ProgrammaticScriptInjector::ShouldInjectJs(
    UserScript::RunLocation run_location) const {
  return GetRunLocation() == run_location && params_->is_javascript;
}

bool ProgrammaticScriptInjector::ShouldInjectCss(
    UserScript::RunLocation run_location) const {
  return GetRunLocation() == run_location && !params_->is_javascript;
}

PermissionsData::AccessType ProgrammaticScriptInjector::CanExecuteOnFrame(
    const Extension* extension,
    blink::WebFrame* frame,
    int tab_id,
    const GURL& top_url) const {
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->document().url(), params_->match_about_blank);
  if (params_->is_web_view) {
    return effective_document_url == params_->webview_src
               ? PermissionsData::ACCESS_ALLOWED
               : PermissionsData::ACCESS_DENIED;
  }

  return extension->permissions_data()->GetPageAccess(extension,
                                                      effective_document_url,
                                                      top_url,
                                                      tab_id,
                                                      -1,  // no process ID.
                                                      NULL /* ignore error */);
}

std::vector<blink::WebScriptSource> ProgrammaticScriptInjector::GetJsSources(
    UserScript::RunLocation run_location) const {
  DCHECK_EQ(GetRunLocation(), run_location);
  DCHECK(params_->is_javascript);

  return std::vector<blink::WebScriptSource>(
      1,
      blink::WebScriptSource(
          blink::WebString::fromUTF8(params_->code), params_->file_url));
}

std::vector<std::string> ProgrammaticScriptInjector::GetCssSources(
    UserScript::RunLocation run_location) const {
  DCHECK_EQ(GetRunLocation(), run_location);
  DCHECK(!params_->is_javascript);

  return std::vector<std::string>(1, params_->code);
}

void ProgrammaticScriptInjector::OnInjectionComplete(
    scoped_ptr<base::ListValue> execution_results,
    ScriptsRunInfo* scripts_run_info,
    UserScript::RunLocation run_location) {
  results_ = execution_results.Pass();
  Finish(std::string());
}

void ProgrammaticScriptInjector::OnWillNotInject(InjectFailureReason reason) {
  std::string error;
  switch (reason) {
    case NOT_ALLOWED:
      error = ErrorUtils::FormatErrorMessage(manifest_errors::kCannotAccessPage,
                                             url_.spec());
      break;
    case EXTENSION_REMOVED:  // no special error here.
    case WONT_INJECT:
      break;
  }
  Finish(error);
}

UserScript::RunLocation ProgrammaticScriptInjector::GetRunLocation() const {
  return static_cast<UserScript::RunLocation>(params_->run_at);
}

void ProgrammaticScriptInjector::Finish(const std::string& error) {
  DCHECK(!finished_);
  finished_ = true;

  render_view_->Send(new ExtensionHostMsg_ExecuteCodeFinished(
      render_view_->GetRoutingID(),
      params_->request_id,
      error,
      url_,
      *results_));
}

}  // namespace extensions
