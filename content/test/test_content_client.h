// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTENT_CLIENT_H_
#define CONTENT_TEST_TEST_CONTENT_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"
#include "ui/base/resource/data_pack.h"

class TestContentClient : public content::ContentClient {
 public:
  TestContentClient();
  virtual ~TestContentClient();

  // content::ContentClient:
  virtual void SetActiveURL(const GURL& url) OVERRIDE;
  virtual void SetGpuInfo(const content::GPUInfo& gpu_info) OVERRIDE;
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) OVERRIDE;
  virtual void AddNPAPIPlugins(
      webkit::npapi::PluginList* plugin_list) OVERRIDE;
  virtual bool CanSendWhileSwappedOut(const IPC::Message* msg) OVERRIDE;
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg) OVERRIDE;
  virtual std::string GetUserAgent(bool* overriding) const OVERRIDE;
  virtual string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(int resource_id) const OVERRIDE;
#if defined(OS_WIN)
  virtual bool SandboxPlugin(CommandLine* command_line,
                             sandbox::TargetPolicy* policy) OVERRIDE;
#endif
#if defined(OS_MACOSX)
  virtual bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const OVERRIDE;
#endif

 private:
  ui::DataPack data_pack_;

  DISALLOW_COPY_AND_ASSIGN(TestContentClient);
};

#endif  // CONTENT_TEST_TEST_CONTENT_CLIENT_H_
