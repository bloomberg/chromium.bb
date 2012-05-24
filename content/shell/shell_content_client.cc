// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_client.h"

#include "base/string_piece.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/user_agent.h"

namespace content {

ShellContentClient::~ShellContentClient() {
}

void ShellContentClient::SetActiveURL(const GURL& url) {
}

void ShellContentClient::SetGpuInfo(const GPUInfo& gpu_info) {
}

void ShellContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
}

void ShellContentClient::AddNPAPIPlugins(
    webkit::npapi::PluginList* plugin_list) {
}

void ShellContentClient::AddAdditionalSchemes(
    std::vector<std::string>* standard_schemes,
    std::vector<std::string>* savable_schemes) {
}

bool ShellContentClient::HasWebUIScheme(const GURL& url) const {
  // There are no WebUI URLs in content_shell.
  return false;
}

bool ShellContentClient::CanHandleWhileSwappedOut(const IPC::Message& msg) {
  return false;
}

std::string ShellContentClient::GetUserAgent() const {
  // The "19" is so that sites that sniff for version think that this is
  // something reasonably current; the "77.34.5" is a hint that this isn't a
  // standard Chrome.
  return webkit_glue::BuildUserAgentFromProduct("Chrome/19.77.34.5");
}

string16 ShellContentClient::GetLocalizedString(int message_id) const {
  return string16();
}

base::StringPiece ShellContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id, scale_factor);
}

#if defined(OS_WIN)
bool ShellContentClient::SandboxPlugin(CommandLine* command_line,
                                       sandbox::TargetPolicy* policy) {
  return false;
}
#endif

#if defined(OS_MACOSX)
bool ShellContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  return false;
}
#endif

}  // namespace content
