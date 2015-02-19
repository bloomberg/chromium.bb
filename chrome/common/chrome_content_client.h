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

#if defined(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#endif

// Returns the user agent of Chrome.
std::string GetUserAgent();

class ChromeContentClient : public content::ContentClient {
 public:
  static const char kPDFPluginName[];
  static const char kPDFPluginPath[];
  static const char kRemotingViewerPluginPath[];

  // The methods below are called by child processes to set the function
  // pointers for built-in plugins. We avoid linking these plugins into
  // chrome_common because then on Windows we would ship them twice because of
  // the split DLL.
#if defined(ENABLE_REMOTING)
  static void SetRemotingEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
#endif

#if !defined(DISABLE_NACL)
  static void SetNaClEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
#endif

#if defined(ENABLE_PLUGINS)
  static void SetPDFEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
#endif

  void SetActiveURL(const GURL& url) override;
  void SetGpuInfo(const gpu::GPUInfo& gpu_info) override;
  void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) override;
  void AddAdditionalSchemes(std::vector<std::string>* standard_schemes,
                            std::vector<std::string>* saveable_shemes) override;
  std::string GetProduct() const override;
  std::string GetUserAgent() const override;
  base::string16 GetLocalizedString(int message_id) const override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override;
  base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const override;
  gfx::Image& GetNativeImageNamed(int resource_id) const override;
  std::string GetProcessTypeNameInEnglish(int type) override;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const override;
#endif
};

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
