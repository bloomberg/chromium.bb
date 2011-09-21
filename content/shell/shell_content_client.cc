// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_client.h"

#include "base/string_piece.h"
#include "webkit/glue/user_agent.h"

namespace content {

ShellContentClient::~ShellContentClient() {
}

void ShellContentClient::SetActiveURL(const GURL& url) {
}

void ShellContentClient::SetGpuInfo(const GPUInfo& gpu_info) {
}

void ShellContentClient::AddPepperPlugins(
    std::vector<PepperPluginInfo>* plugins) {
}

bool ShellContentClient::CanSendWhileSwappedOut(const IPC::Message* msg) {
  return false;
}

bool ShellContentClient::CanHandleWhileSwappedOut(const IPC::Message& msg) {
  return false;
}

std::string ShellContentClient::GetUserAgent(bool mimic_windows) const {
  return webkit_glue::BuildUserAgentHelper(mimic_windows, "Chrome/15.16.17.18");
}

string16 ShellContentClient::GetLocalizedString(int message_id) const {
  return string16();
}

base::StringPiece ShellContentClient::GetDataResource(int resource_id) const {
  return base::StringPiece();
}

#if defined(OS_WIN)
bool ShellContentClient::SandboxPlugin(CommandLine* command_line,
                                       sandbox::TargetPolicy* policy) {
  return false;
}
#endif

}  // namespace content
