// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "content/public/utility/content_utility_client.h"

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

  static void PreSandboxStartup();

 private:
  // IPC message handlers.
  typedef ScopedVector<UtilityMessageHandler> Handlers;
  Handlers handlers_;

  // True if the utility process runs with elevated privileges.
  bool utility_process_running_elevated_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentUtilityClient);
};

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
