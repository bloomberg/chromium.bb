// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_client.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_piece.h"

TestContentClient::TestContentClient() {
  FilePath content_resources_pack_path;
  PathService::Get(base::DIR_MODULE, &content_resources_pack_path);
  content_resources_pack_path = content_resources_pack_path.Append(
      FILE_PATH_LITERAL("content_resources.pak"));
  data_pack_.Load(content_resources_pack_path);
}

TestContentClient::~TestContentClient() {
}

void TestContentClient::SetActiveURL(const GURL& url) {
}

void TestContentClient::SetGpuInfo(const content::GPUInfo& gpu_info) {
}

void TestContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
}

void TestContentClient::AddNPAPIPlugins(
    webkit::npapi::PluginList* plugin_list) {
}

bool TestContentClient::CanSendWhileSwappedOut(const IPC::Message* msg) {
  return true;
}

bool TestContentClient::CanHandleWhileSwappedOut(const IPC::Message& msg) {
  return true;
}

std::string TestContentClient::GetUserAgent(bool* overriding) const {
  *overriding = false;
  return std::string("TestContentClient");
}

string16 TestContentClient::GetLocalizedString(int message_id) const {
  return string16();
}

base::StringPiece TestContentClient::GetDataResource(int resource_id) const {
  base::StringPiece resource;
  data_pack_.GetStringPiece(resource_id, &resource);
  return resource;
}

#if defined(OS_WIN)
bool TestContentClient::SandboxPlugin(CommandLine* command_line,
                                      sandbox::TargetPolicy* policy) {
  return false;
}
#endif

#if defined(OS_MACOSX)
bool TestContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  return false;
}
#endif
