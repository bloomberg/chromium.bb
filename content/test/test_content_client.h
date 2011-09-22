// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTENT_CLIENT_H_
#define CONTENT_TEST_TEST_CONTENT_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/common/content_client.h"

class TestContentClient : public content::ContentClient {
 public:
  TestContentClient();
  virtual ~TestContentClient();

  // content::ContentClient:
  virtual void SetActiveURL(const GURL& url) OVERRIDE;
  virtual void SetGpuInfo(const GPUInfo& gpu_info) OVERRIDE;
  virtual void AddPepperPlugins(
      std::vector<PepperPluginInfo>* plugins) OVERRIDE;
  virtual bool CanSendWhileSwappedOut(const IPC::Message* msg) OVERRIDE;
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg) OVERRIDE;
  virtual std::string GetUserAgent(bool mimic_windows) const OVERRIDE;
  virtual string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(int resource_id) const OVERRIDE;
#if defined(OS_WIN)
  virtual bool SandboxPlugin(CommandLine* command_line,
                             sandbox::TargetPolicy* policy) OVERRIDE;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContentClient);
};

#endif  // CONTENT_TEST_TEST_CONTENT_CLIENT_H_
