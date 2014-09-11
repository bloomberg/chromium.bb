// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_view/web_view_internal_api.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/common/api/web_view_internal.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

using content::WebContents;
using extensions::core_api::web_view_internal::SetPermission::Params;
using extensions::core_api::extension_types::InjectDetails;
namespace webview = extensions::core_api::web_view_internal;

namespace extensions {

bool WebViewInternalExtensionFunction::RunAsync() {
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;

  return RunAsyncSafe(guest);
}

bool WebViewInternalNavigateFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Navigate::Params> params(
      webview::Navigate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string src = params->src;
  guest->NavigateGuest(src);
  return true;
}

WebViewInternalExecuteCodeFunction::WebViewInternalExecuteCodeFunction()
    : guest_instance_id_(0), guest_src_(GURL::EmptyGURL()) {
}

WebViewInternalExecuteCodeFunction::~WebViewInternalExecuteCodeFunction() {
}

bool WebViewInternalExecuteCodeFunction::Init() {
  if (details_.get())
    return true;

  if (!args_->GetInteger(0, &guest_instance_id_))
    return false;

  if (!guest_instance_id_)
    return false;

  std::string src;
  if (!args_->GetString(1, &src))
    return false;

  guest_src_ = GURL(src);
  if (!guest_src_.is_valid())
    return false;

  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(2, &details_value))
    return false;
  scoped_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return false;

  details_ = details.Pass();
  return true;
}

bool WebViewInternalExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebViewInternalExecuteCodeFunction::CanExecuteScriptOnPage() {
  return true;
}

extensions::ScriptExecutor*
WebViewInternalExecuteCodeFunction::GetScriptExecutor() {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest)
    return NULL;

  return guest->script_executor();
}

bool WebViewInternalExecuteCodeFunction::IsWebView() const {
  return true;
}

const GURL& WebViewInternalExecuteCodeFunction::GetWebViewSrc() const {
  return guest_src_;
}

WebViewInternalExecuteScriptFunction::WebViewInternalExecuteScriptFunction() {
}

void WebViewInternalExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
  if (error.empty())
    SetResult(result.DeepCopy());
  WebViewInternalExecuteCodeFunction::OnExecuteCodeFinished(
      error, on_url, result);
}

WebViewInternalInsertCSSFunction::WebViewInternalInsertCSSFunction() {
}

bool WebViewInternalInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

WebViewInternalCaptureVisibleRegionFunction::
    WebViewInternalCaptureVisibleRegionFunction() {
}

WebViewInternalCaptureVisibleRegionFunction::
    ~WebViewInternalCaptureVisibleRegionFunction() {
}

bool WebViewInternalCaptureVisibleRegionFunction::IsScreenshotEnabled() {
  return true;
}

WebContents* WebViewInternalCaptureVisibleRegionFunction::GetWebContentsForID(
    int instance_id) {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  return guest ? guest->web_contents() : NULL;
}

void WebViewInternalCaptureVisibleRegionFunction::OnCaptureFailure(
    FailureReason reason) {
  SendResponse(false);
}

WebViewInternalSetNameFunction::WebViewInternalSetNameFunction() {
}

WebViewInternalSetNameFunction::~WebViewInternalSetNameFunction() {
}

WebViewInternalSetZoomFunction::WebViewInternalSetZoomFunction() {
}

WebViewInternalSetZoomFunction::~WebViewInternalSetZoomFunction() {
}

bool WebViewInternalSetNameFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetName::Params> params(
      webview::SetName::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetName(params->frame_name);
  SendResponse(true);
  return true;
}

bool WebViewInternalSetZoomFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetZoom::Params> params(
      webview::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetZoom(params->zoom_factor);

  SendResponse(true);
  return true;
}

WebViewInternalGetZoomFunction::WebViewInternalGetZoomFunction() {
}

WebViewInternalGetZoomFunction::~WebViewInternalGetZoomFunction() {
}

bool WebViewInternalGetZoomFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::GetZoom::Params> params(
      webview::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double zoom_factor = guest->GetZoom();
  SetResult(new base::FundamentalValue(zoom_factor));
  SendResponse(true);
  return true;
}

WebViewInternalFindFunction::WebViewInternalFindFunction() {
}

WebViewInternalFindFunction::~WebViewInternalFindFunction() {
}

bool WebViewInternalFindFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Find::Params> params(
      webview::Find::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Convert the std::string search_text to string16.
  base::string16 search_text;
  base::UTF8ToUTF16(
      params->search_text.c_str(), params->search_text.length(), &search_text);

  // Set the find options to their default values.
  blink::WebFindOptions options;
  if (params->options) {
    options.forward =
        params->options->backward ? !*params->options->backward : true;
    options.matchCase =
        params->options->match_case ? *params->options->match_case : false;
  }

  guest->Find(search_text, options, this);
  return true;
}

WebViewInternalStopFindingFunction::WebViewInternalStopFindingFunction() {
}

WebViewInternalStopFindingFunction::~WebViewInternalStopFindingFunction() {
}

bool WebViewInternalStopFindingFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::StopFinding::Params> params(
      webview::StopFinding::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Set the StopFindAction.
  content::StopFindAction action;
  switch (params->action) {
    case webview::StopFinding::Params::ACTION_CLEAR:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case webview::StopFinding::Params::ACTION_KEEP:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case webview::StopFinding::Params::ACTION_ACTIVATE:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }

  guest->StopFinding(action);
  return true;
}

WebViewInternalGoFunction::WebViewInternalGoFunction() {
}

WebViewInternalGoFunction::~WebViewInternalGoFunction() {
}

bool WebViewInternalGoFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::Go::Params> params(webview::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->Go(params->relative_index);
  return true;
}

WebViewInternalReloadFunction::WebViewInternalReloadFunction() {
}

WebViewInternalReloadFunction::~WebViewInternalReloadFunction() {
}

bool WebViewInternalReloadFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Reload();
  return true;
}

WebViewInternalSetPermissionFunction::WebViewInternalSetPermissionFunction() {
}

WebViewInternalSetPermissionFunction::~WebViewInternalSetPermissionFunction() {
}

bool WebViewInternalSetPermissionFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<webview::SetPermission::Params> params(
      webview::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewPermissionHelper::PermissionResponseAction action =
      WebViewPermissionHelper::DEFAULT;
  switch (params->action) {
    case Params::ACTION_ALLOW:
      action = WebViewPermissionHelper::ALLOW;
      break;
    case Params::ACTION_DENY:
      action = WebViewPermissionHelper::DENY;
      break;
    case Params::ACTION_DEFAULT:
      break;
    default:
      NOTREACHED();
  }

  std::string user_input;
  if (params->user_input)
    user_input = *params->user_input;

  WebViewPermissionHelper* web_view_permission_helper =
      WebViewPermissionHelper::FromWebContents(guest->web_contents());

  WebViewPermissionHelper::SetPermissionResult result =
      web_view_permission_helper->SetPermission(
          params->request_id, action, user_input);

  EXTENSION_FUNCTION_VALIDATE(result !=
                              WebViewPermissionHelper::SET_PERMISSION_INVALID);

  SetResult(new base::FundamentalValue(
      result == WebViewPermissionHelper::SET_PERMISSION_ALLOWED));
  SendResponse(true);
  return true;
}

WebViewInternalOverrideUserAgentFunction::
    WebViewInternalOverrideUserAgentFunction() {
}

WebViewInternalOverrideUserAgentFunction::
    ~WebViewInternalOverrideUserAgentFunction() {
}

bool WebViewInternalOverrideUserAgentFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<webview::OverrideUserAgent::Params> params(
      webview::OverrideUserAgent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->SetUserAgentOverride(params->user_agent_override);
  return true;
}

WebViewInternalStopFunction::WebViewInternalStopFunction() {
}

WebViewInternalStopFunction::~WebViewInternalStopFunction() {
}

bool WebViewInternalStopFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Stop();
  return true;
}

WebViewInternalTerminateFunction::WebViewInternalTerminateFunction() {
}

WebViewInternalTerminateFunction::~WebViewInternalTerminateFunction() {
}

bool WebViewInternalTerminateFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Terminate();
  return true;
}

}  // namespace extensions
