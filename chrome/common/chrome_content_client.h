// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#pragma once

#include <string>
#include <vector>

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
  virtual void AddAdditionalSchemes(
      std::vector<std::string>* standard_schemes,
      std::vector<std::string>* saveable_shemes) OVERRIDE;
  virtual bool HasWebUIScheme(const GURL& url) const OVERRIDE;
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg) OVERRIDE;
  virtual std::string GetUserAgent() const OVERRIDE;
  virtual string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const OVERRIDE;

#if defined(OS_WIN)
  virtual bool SandboxPlugin(CommandLine* command_line,
                             sandbox::TargetPolicy* policy) OVERRIDE;
#endif

#if defined(OS_MACOSX)
  virtual bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const OVERRIDE;
#endif

  // Gets information about the bundled Pepper Flash for field trial.
  // |override_npapi_flash| indicates whether it should take precedence over
  // the internal NPAPI Flash.
  // Returns false if bundled Pepper Flash is not available. In that case,
  // |plugin| and |override_npapi_flash| are not touched.
  //
  // TODO(yzshen): We need this method because currently we are having a field
  // trial with bundled Pepper Flash, and need extra information about how to
  // order bundled Pepper Flash and internal NPAPI Flash. Once the field trial
  // is over, we should merge this into AddPepperPlugins().
  bool GetBundledFieldTrialPepperFlash(content::PepperPluginInfo* plugin,
                                       bool* override_npapi_flash);
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
