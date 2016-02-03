// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/programmatic_script_injector.h"

#include <utility>
#include <vector>

#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "url/origin.h"

namespace extensions {

ProgrammaticScriptInjector::ProgrammaticScriptInjector(
    const ExtensionMsg_ExecuteCode_Params& params,
    content::RenderFrame* render_frame)
    : params_(new ExtensionMsg_ExecuteCode_Params(params)),
      url_(
          ScriptContext::GetDataSourceURLForFrame(render_frame->GetWebFrame())),
      finished_(false) {
  effective_url_ = ScriptContext::GetEffectiveDocumentURL(
      render_frame->GetWebFrame(), url_, params.match_about_blank);
}

ProgrammaticScriptInjector::~ProgrammaticScriptInjector() {
}

UserScript::InjectionType ProgrammaticScriptInjector::script_type()
    const {
  return UserScript::PROGRAMMATIC_SCRIPT;
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
    const InjectionHost* injection_host,
    blink::WebLocalFrame* frame,
    int tab_id) const {
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->document().url(), params_->match_about_blank);
  if (params_->is_web_view) {
    if (frame->parent()) {
      // This is a subframe inside <webview>, so allow it.
      return PermissionsData::ACCESS_ALLOWED;
    }

    return effective_document_url == params_->webview_src
               ? PermissionsData::ACCESS_ALLOWED
               : PermissionsData::ACCESS_DENIED;
  }
  DCHECK_EQ(injection_host->id().type(), HostID::EXTENSIONS);

  return injection_host->CanExecuteOnFrame(
      effective_document_url,
      content::RenderFrame::FromWebFrame(frame),
      tab_id,
      true /* is_declarative */);
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

void ProgrammaticScriptInjector::GetRunInfo(
    ScriptsRunInfo* scripts_run_info,
    UserScript::RunLocation run_location) const {
}

void ProgrammaticScriptInjector::OnInjectionComplete(
    scoped_ptr<base::Value> execution_result,
    UserScript::RunLocation run_location,
    content::RenderFrame* render_frame) {
  DCHECK(results_.empty());
  if (execution_result)
    results_.Append(std::move(execution_result));
  Finish(std::string(), render_frame);
}

void ProgrammaticScriptInjector::OnWillNotInject(
    InjectFailureReason reason,
    content::RenderFrame* render_frame) {
  std::string error;
  switch (reason) {
    case NOT_ALLOWED:
      if (url_.SchemeIs(url::kAboutScheme)) {
        error = ErrorUtils::FormatErrorMessage(
            manifest_errors::kCannotAccessAboutUrl, url_.spec(),
            url::Origin(effective_url_).Serialize());
      } else {
        // TODO(?) It would be nice to show kCannotAccessPageWithUrl here if
        // this is triggered by an extension with tabs permission. See
        // https://codereview.chromium.org/1414223005/diff/1/extensions/
        // common/manifest_constants.cc#newcode269
        error = manifest_errors::kCannotAccessPage;
      }
      break;
    case EXTENSION_REMOVED:  // no special error here.
    case WONT_INJECT:
      break;
  }
  Finish(error, render_frame);
}

UserScript::RunLocation ProgrammaticScriptInjector::GetRunLocation() const {
  return static_cast<UserScript::RunLocation>(params_->run_at);
}

void ProgrammaticScriptInjector::Finish(const std::string& error,
                                        content::RenderFrame* render_frame) {
  DCHECK(!finished_);
  finished_ = true;

  // It's possible that the render frame was destroyed in the course of
  // injecting scripts. Don't respond if it was (the browser side watches for
  // frame deletions so nothing is left hanging).
  if (render_frame) {
    render_frame->Send(
        new ExtensionHostMsg_ExecuteCodeFinished(
            render_frame->GetRoutingID(), params_->request_id,
            error, url_, results_));
  }
}

}  // namespace extensions
