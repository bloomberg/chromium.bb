// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_client.h"

#include "base/logging.h"
#include "base/string_piece.h"

TestContentClient::TestContentClient() {
}

TestContentClient::~TestContentClient() {
}

void TestContentClient::SetActiveURL(const GURL& url) {
}

void TestContentClient::SetGpuInfo(const GPUInfo& gpu_info) {
}

void TestContentClient::AddPepperPlugins(
    std::vector<PepperPluginInfo>* plugins) {
}

bool TestContentClient::CanSendWhileSwappedOut(const IPC::Message* msg) {
  return true;
}

bool TestContentClient::CanHandleWhileSwappedOut(const IPC::Message& msg) {
  return true;
}

std::string TestContentClient::GetUserAgent(bool mimic_windows) const {
  return std::string();
}

string16 TestContentClient::GetLocalizedString(int message_id) const {
  return string16();
}

base::StringPiece TestContentClient::GetDataResource(int resource_id) const {
  return base::StringPiece();
}

#if defined(OS_WIN)
bool TestContentClient::SandboxPlugin(CommandLine* command_line,
                                      sandbox::TargetPolicy* policy) {
  return false;
}
#endif
