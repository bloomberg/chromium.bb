// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webview/webview_api.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/common/extensions/api/webview.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

using extensions::api::tabs::InjectDetails;
namespace webview = extensions::api::webview;

WebviewExecuteCodeFunction::WebviewExecuteCodeFunction()
    : guest_instance_id_(0) {
}

WebviewExecuteCodeFunction::~WebviewExecuteCodeFunction() {
}

bool WebviewExecuteCodeFunction::Init() {
  if (details_.get())
    return true;

  if (!args_->GetInteger(0, &guest_instance_id_))
    return false;

  if (!guest_instance_id_)
    return false;

  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return false;
  scoped_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return false;

  details_ = details.Pass();
  return true;
}

bool WebviewExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebviewExecuteCodeFunction::CanExecuteScriptOnPage() {
  return true;
}

extensions::ScriptExecutor* WebviewExecuteCodeFunction::GetScriptExecutor() {
  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest)
    return NULL;

  return guest->script_executor();
}

bool WebviewExecuteCodeFunction::IsWebView() const {
  return true;
}

WebviewExecuteScriptFunction::WebviewExecuteScriptFunction() {
}

void WebviewExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    int32 on_page_id,
    const GURL& on_url,
    const base::ListValue& result) {
  content::RecordAction(content::UserMetricsAction("WebView.ExecuteScript"));
  if (error.empty())
    SetResult(result.DeepCopy());
  WebviewExecuteCodeFunction::OnExecuteCodeFinished(error, on_page_id, on_url,
                                                    result);
}

WebviewInsertCSSFunction::WebviewInsertCSSFunction() {
}

bool WebviewInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

WebviewGoFunction::WebviewGoFunction() {
}

WebviewGoFunction::~WebviewGoFunction() {
}

bool WebviewGoFunction::RunImpl() {
  scoped_ptr<webview::Go::Params> params(webview::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Go(params->relative_index);
  return true;
}

WebviewReloadFunction::WebviewReloadFunction() {
}

WebviewReloadFunction::~WebviewReloadFunction() {
}

bool WebviewReloadFunction::RunImpl() {
  scoped_ptr<webview::Reload::Params> params(
      webview::Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Reload();
  return true;
}

WebviewSetPermissionFunction::WebviewSetPermissionFunction() {
}

WebviewSetPermissionFunction::~WebviewSetPermissionFunction() {
}

bool WebviewSetPermissionFunction::RunImpl() {
  scoped_ptr<webview::SetPermission::Params> params(
      webview::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  EXTENSION_FUNCTION_VALIDATE(
      guest->SetPermission(params->request_id,
                           params->should_allow,
                           params->user_input));
  return true;
}

WebviewStopFunction::WebviewStopFunction() {
}

WebviewStopFunction::~WebviewStopFunction() {
}

bool WebviewStopFunction::RunImpl() {
  scoped_ptr<webview::Stop::Params> params(
      webview::Stop::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Stop();
  return true;
}

WebviewTerminateFunction::WebviewTerminateFunction() {
}

WebviewTerminateFunction::~WebviewTerminateFunction() {
}

bool WebviewTerminateFunction::RunImpl() {
  scoped_ptr<webview::Terminate::Params> params(
      webview::Terminate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewGuest* guest = WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;

  guest->Terminate();
  return true;
}
