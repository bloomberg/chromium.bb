// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

namespace {

const char kFileInputCommand[] = "DOM.setFileInputFiles";
const char kFileInputFilesParam[] = "files";
const char kHandleDialogCommand[] = "Page.handleJavaScriptDialog";
const char kHandleDialogAcceptParam[] = "accept";

}  // namespace

RendererOverridesHandler::RendererOverridesHandler(DevToolsAgentHost* agent)
    : agent_(agent) {
  RegisterCommandHandler(
      kFileInputCommand,
      base::Bind(
          &RendererOverridesHandler::GrantPermissionsForSetFileInputFiles,
          base::Unretained(this)));
  RegisterCommandHandler(
      kHandleDialogCommand,
      base::Bind(
          &RendererOverridesHandler::HandleJavaScriptDialog,
          base::Unretained(this)));
}

RendererOverridesHandler::~RendererOverridesHandler() {}

scoped_ptr<DevToolsProtocol::Response>
RendererOverridesHandler::GrantPermissionsForSetFileInputFiles(
    DevToolsProtocol::Command* command) {
  const base::ListValue* file_list;
  if (!command->params()->GetList(kFileInputFilesParam, &file_list)) {
    return command->ErrorResponse(
        DevToolsProtocol::kErrorInvalidParams,
        base::StringPrintf("Missing or invalid '%s' parameter",
                           kFileInputFilesParam));
  }
  for (size_t i = 0; i < file_list->GetSize(); ++i) {
    base::FilePath::StringType file;
    if (!file_list->GetString(i, &file)) {
      return command->ErrorResponse(
          DevToolsProtocol::kErrorInvalidParams,
          base::StringPrintf("'%s' must be a list of strings",
                             kFileInputFilesParam));
    }
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        agent_->GetRenderViewHost()->GetProcess()->GetID(),
        base::FilePath(file));
  }
  return scoped_ptr<DevToolsProtocol::Response>();
}

scoped_ptr<DevToolsProtocol::Response>
RendererOverridesHandler::HandleJavaScriptDialog(
    DevToolsProtocol::Command* command) {
  bool accept;
  if (!command->params()->GetBoolean(kHandleDialogAcceptParam, &accept)) {
    return command->ErrorResponse(
        DevToolsProtocol::kErrorInvalidParams,
        base::StringPrintf("Missing or invalid '%s' parameter",
                           kHandleDialogAcceptParam));
  }

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      agent_->GetRenderViewHost()->GetDelegate()->GetAsWebContents());
  if (web_contents) {
    JavaScriptDialogManager* manager =
        web_contents->GetDelegate()->GetJavaScriptDialogManager();
    if (manager && manager->HandleJavaScriptDialog(web_contents, accept))
      return scoped_ptr<DevToolsProtocol::Response>();
  }
  return command->ErrorResponse(
      DevToolsProtocol::kErrorInternalError,
      "No JavaScript dialog to handle");
}

}  // namespace content
