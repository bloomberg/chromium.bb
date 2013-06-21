// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webview/webview_api.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/webview/webview_guest.h"
#include "chrome/common/extensions/api/webview.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

using extensions::api::tabs::InjectDetails;

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
  chrome::WebViewGuest* guest = chrome::WebViewGuest::From(
      render_view_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest)
    return NULL;

  return guest->script_executor();
}

bool WebviewExecuteCodeFunction::IsWebView() const {
  return true;
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

bool WebviewInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

