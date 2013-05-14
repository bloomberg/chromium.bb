// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webview/webview_api.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/api/webview.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

using extensions::api::tabs::InjectDetails;

WebviewExecuteCodeFunction::WebviewExecuteCodeFunction()
    : process_id_(0),
      route_id_(MSG_ROUTING_NONE) {
}

WebviewExecuteCodeFunction::~WebviewExecuteCodeFunction() {
}

bool WebviewExecuteCodeFunction::Init() {
  if (details_.get())
    return true;

  if (!args_->GetInteger(0, &process_id_))
    return false;

  if (!args_->GetInteger(1, &route_id_) || route_id_ == MSG_ROUTING_NONE)
    return false;

  DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(2, &details_value))
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
  content::RenderViewHost* guest_rvh =
      content::RenderViewHost::FromID(process_id_, route_id_);
  // If we haven't loaded a guest yet, then this will be NULL.
  if (!guest_rvh)
    return NULL;

  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(guest_rvh);
  if (!contents)
    return NULL;

  return extensions::TabHelper::FromWebContents(contents)->script_executor();
}

bool WebviewExecuteCodeFunction::IsWebView() const {
  return true;
}

void WebviewExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    int32 on_page_id,
    const GURL& on_url,
    const ListValue& result) {
  if (error.empty())
    SetResult(result.DeepCopy());
  WebviewExecuteCodeFunction::OnExecuteCodeFinished(error, on_page_id, on_url,
                                                    result);
}

bool WebviewInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

