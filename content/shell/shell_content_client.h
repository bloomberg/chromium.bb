// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_CONTENT_CLIENT_H_
#define CONTENT_SHELL_SHELL_CONTENT_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"

namespace content {

class ShellContentClient : public ContentClient {
 public:
  virtual ~ShellContentClient();

  virtual void SetActiveURL(const GURL& url) OVERRIDE;
  virtual void SetGpuInfo(const GPUInfo& gpu_info) OVERRIDE;
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) OVERRIDE;
  virtual void AddNPAPIPlugins(
      webkit::npapi::PluginList* plugin_list) OVERRIDE;
  virtual void AddAdditionalSchemes(
      std::vector<std::string>* standard_schemes,
      std::vector<std::string>* savable_schemes) OVERRIDE;
  virtual bool HasWebUIScheme(const GURL& url) const OVERRIDE;
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg) OVERRIDE;
  virtual std::string GetUserAgent() const OVERRIDE;
  virtual string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(int resource_id) const OVERRIDE;
  virtual base::StringPiece GetImageResource(int resource_id,
                                             float scale_factor) const OVERRIDE;

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

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_CONTENT_CLIENT_H_
