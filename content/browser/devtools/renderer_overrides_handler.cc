// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

namespace content {

namespace {

const char kFileInputCommand[] = "DOM.setFileInputFiles";
const char kFileInputFilesParam[] = "files";

}  // namespace

RendererOverridesHandler::RendererOverridesHandler(DevToolsAgentHost* agent)
    : agent_(agent) {
  RegisterCommandHandler(
      kFileInputCommand,
      base::Bind(
          &RendererOverridesHandler::GrantPermissionsForSetFileInputFiles,
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

}  // namespace content
