// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"

namespace content {

namespace {

const char kDOMFileInputCommand[] = "DOM.setFileInputFiles";
const char kDOMFileInputFilesParam[] = "files";
const char kPageHandleDialogCommand[] = "Page.handleJavaScriptDialog";
const char kPageHandleDialogAcceptParam[] = "accept";
const char kPageHandleDialogPromptTextParam[] = "promptText";
const char kPageNavigateCommand[] = "Page.navigate";
const char kPageNavigateUrlParam[] = "url";

}  // namespace

RendererOverridesHandler::RendererOverridesHandler(DevToolsAgentHost* agent)
    : agent_(agent) {
  RegisterCommandHandler(
      kDOMFileInputCommand,
      base::Bind(
          &RendererOverridesHandler::GrantPermissionsForSetFileInputFiles,
          base::Unretained(this)));
  RegisterCommandHandler(
      kPageHandleDialogCommand,
      base::Bind(
          &RendererOverridesHandler::PageHandleJavaScriptDialog,
          base::Unretained(this)));
  RegisterCommandHandler(
      kPageNavigateCommand,
      base::Bind(
          &RendererOverridesHandler::PageNavigate,
          base::Unretained(this)));
}

RendererOverridesHandler::~RendererOverridesHandler() {}

scoped_ptr<DevToolsProtocol::Response>
RendererOverridesHandler::GrantPermissionsForSetFileInputFiles(
    DevToolsProtocol::Command* command) {
  base::DictionaryValue* params = command->params();
  base::ListValue* file_list;
  if (!params || !params->GetList(kDOMFileInputFilesParam, &file_list)) {
    return command->ErrorResponse(
        DevToolsProtocol::kErrorInvalidParams,
        base::StringPrintf("Missing or invalid '%s' parameter",
                           kDOMFileInputFilesParam));
  }
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (!host)
    return scoped_ptr<DevToolsProtocol::Response>();

  for (size_t i = 0; i < file_list->GetSize(); ++i) {
    base::FilePath::StringType file;
    if (!file_list->GetString(i, &file)) {
      return command->ErrorResponse(
          DevToolsProtocol::kErrorInvalidParams,
          base::StringPrintf("'%s' must be a list of strings",
                             kDOMFileInputFilesParam));
    }
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        host->GetProcess()->GetID(), base::FilePath(file));
  }
  return scoped_ptr<DevToolsProtocol::Response>();
}

scoped_ptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageHandleJavaScriptDialog(
    DevToolsProtocol::Command* command) {
  base::DictionaryValue* params = command->params();
  bool accept;
  if (!params || !params->GetBoolean(kPageHandleDialogAcceptParam, &accept)) {
    return command->ErrorResponse(
        DevToolsProtocol::kErrorInvalidParams,
        base::StringPrintf("Missing or invalid '%s' parameter",
                           kPageHandleDialogAcceptParam));
  }
  string16 prompt_override;
  string16* prompt_override_ptr = &prompt_override;
  if (!params || !params->GetString(kPageHandleDialogPromptTextParam,
                                    prompt_override_ptr)) {
    prompt_override_ptr = NULL;
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      JavaScriptDialogManager* manager =
          web_contents->GetDelegate()->GetJavaScriptDialogManager();
      if (manager && manager->HandleJavaScriptDialog(
              web_contents, accept, prompt_override_ptr)) {
        return scoped_ptr<DevToolsProtocol::Response>();
      }
    }
  }
  return command->ErrorResponse(
      DevToolsProtocol::kErrorInternalError,
      "No JavaScript dialog to handle");
}

scoped_ptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageNavigate(
    DevToolsProtocol::Command* command) {
  base::DictionaryValue* params = command->params();
  std::string url;
  if (!params || !params->GetString(kPageNavigateUrlParam, &url)) {
    return command->ErrorResponse(
        DevToolsProtocol::kErrorInvalidParams,
        base::StringPrintf("Missing or invalid '%s' parameter",
                           kPageNavigateUrlParam));
  }
  GURL gurl(url);
  if (!gurl.is_valid()) {
    return command->ErrorResponse(
        DevToolsProtocol::kErrorInternalError,
        "Cannot navigate to invalid URL");
  }
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      web_contents->GetController().LoadURL(
          gurl, Referrer(), PAGE_TRANSITION_TYPED, "");
      return command->SuccessResponse(new base::DictionaryValue());
    }
  }
  return command->ErrorResponse(
      DevToolsProtocol::kErrorInternalError,
      "No WebContents to navigate");
}

}  // namespace content
