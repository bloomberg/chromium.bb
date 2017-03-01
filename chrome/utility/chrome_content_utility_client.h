// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "build/build_config.h"
#include "content/public/utility/content_utility_client.h"
#include "ipc/ipc_platform_file.h"

class UtilityMessageHandler;

class ChromeContentUtilityClient : public content::ContentUtilityClient {
 public:
  ChromeContentUtilityClient();
  ~ChromeContentUtilityClient() override;

  // content::ContentUtilityClient:
  void UtilityThreadStarted() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void ExposeInterfacesToBrowser(
      service_manager::InterfaceRegistry* registry) override;
  void RegisterServices(StaticServiceMap* services) override;

  static void PreSandboxStartup();

 private:
  // IPC message handlers.
  void OnStartupPing();
#if defined(FULL_SAFE_BROWSING)
  void OnAnalyzeZipFileForDownloadProtection(
      const IPC::PlatformFileForTransit& zip_file,
      const IPC::PlatformFileForTransit& temp_file);
#if defined(OS_MACOSX)
  void OnAnalyzeDmgFileForDownloadProtection(
      const IPC::PlatformFileForTransit& dmg_file);
#endif  // defined(OS_MACOSX)
#endif  // defined(FULL_SAFE_BROWSING)

  typedef ScopedVector<UtilityMessageHandler> Handlers;
  Handlers handlers_;

  bool utility_process_running_elevated_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentUtilityClient);
};

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
