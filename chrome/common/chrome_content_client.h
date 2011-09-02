// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#pragma once

#include "content/common/content_client.h"

namespace chrome {

class ChromeContentClient : public content::ContentClient {
 public:
  static const char* const kPDFPluginName;
  static const char* const kNaClPluginName;
  static const char* const kNaClOldPluginName;

  virtual void SetActiveURL(const GURL& url);
  virtual void SetGpuInfo(const GPUInfo& gpu_info);
  virtual void AddPepperPlugins(std::vector<PepperPluginInfo>* plugins);
  virtual bool CanSendWhileSwappedOut(const IPC::Message* msg);
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg);
  virtual std::string GetUserAgent(bool mimic_windows) const;
  virtual string16 GetLocalizedString(int message_id) const;
  virtual base::StringPiece GetDataResource(int resource_id) const;

#if defined(OS_WIN)
  virtual bool SandboxPlugin(CommandLine* command_line,
                             sandbox::TargetPolicy* policy);
#endif
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
