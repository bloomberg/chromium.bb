// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "content/public/common/content_client.h"

// Returns the user agent of Chrome.
std::string GetUserAgent();

class ChromeContentClient : public content::ContentClient {
 public:
  static const char* const kPDFPluginName;
  static const char* const kNaClPluginName;
  static const char* const kNaClOldPluginName;
  static const char* const kRemotingViewerPluginPath;

  virtual void SetActiveURL(const GURL& url) OVERRIDE;
  virtual void SetGpuInfo(const gpu::GPUInfo& gpu_info) OVERRIDE;
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) OVERRIDE;
  virtual void AddAdditionalSchemes(
      std::vector<std::string>* standard_schemes,
      std::vector<std::string>* saveable_shemes) OVERRIDE;
  virtual std::string GetProduct() const OVERRIDE;
  virtual std::string GetUserAgent() const OVERRIDE;
  virtual base::string16 GetLocalizedString(int message_id) const OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const OVERRIDE;
  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const OVERRIDE;
  virtual gfx::Image& GetNativeImageNamed(int resource_id) const OVERRIDE;
  virtual std::string GetProcessTypeNameInEnglish(int type) OVERRIDE;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  virtual bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const OVERRIDE;
#endif
};

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
