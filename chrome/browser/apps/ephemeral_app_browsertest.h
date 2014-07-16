// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_BROWSERTEST_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_BROWSERTEST_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "extensions/common/manifest.h"

namespace base {
class CommandLine;
}

// Contains common code for ephemeral app browser tests.
class EphemeralAppTestBase : public extensions::PlatformAppBrowserTest {
 public:
  static const char kMessagingReceiverApp[];
  static const char kMessagingReceiverAppV2[];
  static const char kDispatchEventTestApp[];

  EphemeralAppTestBase();
  virtual ~EphemeralAppTestBase();

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;

 protected:
  base::FilePath GetTestPath(const char* test_path);

  const extensions::Extension* InstallEphemeralApp(
      const char* test_path, extensions::Manifest::Location manifest_location);
  const extensions::Extension* InstallEphemeralApp(const char* test_path);
  const extensions::Extension* InstallAndLaunchEphemeralApp(
      const char* test_path);
  const extensions::Extension*  UpdateEphemeralApp(
      const std::string& app_id,
      const base::FilePath& test_dir,
      const base::FilePath& pem_path);
  void PromoteEphemeralApp(const extensions::Extension* app);

  void CloseApp(const std::string& app_id);
  void EvictApp(const std::string& app_id);
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_BROWSERTEST_H_
