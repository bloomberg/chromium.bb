// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webview/webview_api.h"

#include "chrome/common/extensions/api/webview.h"
#include "content/public/browser/render_view_host.h"

using namespace extensions::api::webview;

WebviewExecuteScriptFunction::WebviewExecuteScriptFunction() {
}

WebviewExecuteScriptFunction::~WebviewExecuteScriptFunction() {
}

bool WebviewExecuteScriptFunction::RunImpl() {
  scoped_ptr<ExecuteScript::Params> params(
      extensions::api::webview::ExecuteScript::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderViewHost* guest_rvh =
      content::RenderViewHost::FromID(params->process_id, params->route_id);
  // If we haven't loaded a guest yet, then this will be NULL.
  if (!guest_rvh)
    return false;
  content::WebContents* guest_web_contents =
      content::WebContents::FromRenderViewHost(guest_rvh);
  CHECK(guest_web_contents);

  ObserverList<extensions::TabHelper::ScriptExecutionObserver>
      script_observers;
  scoped_ptr<extensions::ScriptExecutor> script_executor(
      new extensions::ScriptExecutor(guest_web_contents, &script_observers));

  extensions::ScriptExecutor::FrameScope frame_scope =
      params->details.all_frames.get() && *params->details.all_frames ?
          extensions::ScriptExecutor::ALL_FRAMES :
          extensions::ScriptExecutor::TOP_FRAME;

  extensions::UserScript::RunLocation run_at =
      extensions::UserScript::UNDEFINED;
  switch (params->details.run_at) {
    case InjectDetails::RUN_AT_NONE:
    case InjectDetails::RUN_AT_DOCUMENT_IDLE:
      run_at = extensions::UserScript::DOCUMENT_IDLE;
      break;
    case InjectDetails::RUN_AT_DOCUMENT_START:
      run_at = extensions::UserScript::DOCUMENT_START;
      break;
    case InjectDetails::RUN_AT_DOCUMENT_END:
      run_at = extensions::UserScript::DOCUMENT_END;
      break;
  }
  CHECK_NE(extensions::UserScript::UNDEFINED, run_at);

  script_executor->ExecuteScript(
      GetExtension()->id(),
      extensions::ScriptExecutor::JAVASCRIPT,
      *params->details.code.get(),
      frame_scope,
      run_at,
      extensions::ScriptExecutor::ISOLATED_WORLD,
      true /* is_web_view */,
      base::Bind(
          &WebviewExecuteScriptFunction::OnExecuteCodeFinished,
          this));

  // Balanced in OnExecuteCodeFinished.
  AddRef();
  return true;
}

void WebviewExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    int32 on_page_id,
    const GURL& on_url,
    const ListValue& result) {
  if (error.empty()) {
    SetResult(result.DeepCopy());
  } else {
    SetError(error);
  }
  SendResponse(error.empty());
  Release();  // Added in RunImpl().
}

