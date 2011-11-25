// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"

namespace chrome {

class ChromeContentClient : public content::ContentClient {
 public:
  static const char* const kPDFPluginName;
  static const char* const kNaClPluginName;
  static const char* const kNaClOldPluginName;

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
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
