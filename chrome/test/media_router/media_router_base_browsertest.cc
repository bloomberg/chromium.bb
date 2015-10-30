// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_base_browsertest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"


namespace {
// Command line argument to specify CRX extension location.
const char kExtensionCrx[] = "extension-crx";
// Command line argument to specify unpacked extension location.
const char kExtensionUnpacked[] = "extension-unpacked";
}  // namespace


namespace media_router {

MediaRouterBaseBrowserTest::MediaRouterBaseBrowserTest()
    : extension_load_event_(false, false),
      extension_host_created_(false),
      feature_override_(extensions::FeatureSwitch::media_router(), true) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableExperimentalWebPlatformFeatures, "Presentation");
}

MediaRouterBaseBrowserTest::~MediaRouterBaseBrowserTest() {
}

void MediaRouterBaseBrowserTest::SetUp() {
  ParseCommandLine();
  ExtensionBrowserTest::SetUp();
}

void MediaRouterBaseBrowserTest::TearDown() {
  ExtensionBrowserTest::TearDown();
}

void MediaRouterBaseBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  DCHECK(process_manager);
  process_manager->AddObserver(this);
  InstallAndEnableMRExtension();
  extension_load_event_.Wait();
}

void MediaRouterBaseBrowserTest::TearDownOnMainThread() {
  UninstallMRExtension();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  DCHECK(process_manager);
  process_manager->RemoveObserver(this);
  ExtensionBrowserTest::TearDownOnMainThread();
}

void MediaRouterBaseBrowserTest::InstallAndEnableMRExtension() {
  if (is_unpacked()) {
    const extensions::Extension* extension = LoadExtension(extension_unpacked_);
    extension_id_ = extension->id();
  } else {
    NOTIMPLEMENTED();
  }
}

void MediaRouterBaseBrowserTest::UninstallMRExtension() {
  if (!extension_id_.empty()) {
    UninstallExtension(extension_id_);
  }
}

bool MediaRouterBaseBrowserTest::ConditionalWait(
    base::TimeDelta timeout,
    base::TimeDelta interval,
    const base::Callback<bool(void)>& callback) {
  base::ElapsedTimer timer;
  do {
    if (callback.Run())
      return true;

    base::RunLoop run_loop;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), interval);
    run_loop.Run();
  } while (timer.Elapsed() < timeout);

  return false;
}

void MediaRouterBaseBrowserTest::Wait(base::TimeDelta timeout) {
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), timeout);
  run_loop.Run();
}

void MediaRouterBaseBrowserTest::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  extension_host_created_ = true;
  DVLOG(0) << "Host created";
  extension_load_event_.Signal();
}

void MediaRouterBaseBrowserTest::ParseCommandLine() {
  DVLOG(0) << "ParseCommandLine";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  extension_crx_ = command_line->GetSwitchValuePath(kExtensionCrx);
  extension_unpacked_ = command_line->GetSwitchValuePath(kExtensionUnpacked);

  // Check if there is mr_extension folder under PRODUCT_DIR folder.
  if (extension_crx_.empty() && extension_unpacked_.empty()) {
    base::FilePath base_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &base_dir));
    base::FilePath extension_path =
        base_dir.Append(FILE_PATH_LITERAL("mr_extension/"));
    if (PathExists(extension_path)) {
      extension_unpacked_ = extension_path;
    }
  }

  // Exactly one of these two arguments should be provided.
  ASSERT_NE(extension_crx_.empty(), extension_unpacked_.empty());
}

}  // namespace media_router
