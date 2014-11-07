// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/base/athena_browser_test.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/public/web_contents_view_delegate_creator.h"
#include "athena/env/public/athena_env.h"
#include "athena/main/athena_content_client.h"
#include "athena/main/athena_renderer_pdf_helper.h"
#include "athena/main/public/athena_launcher.h"
#include "athena/test/base/test_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/shell/browser/shell_content_browser_client.h"

namespace athena {
namespace test {

AthenaBrowserTest::AthenaBrowserTest() {
}

AthenaBrowserTest::~AthenaBrowserTest() {
}

content::BrowserContext* AthenaBrowserTest::GetBrowserContext() {
  return extensions::ShellContentBrowserClient::Get()->GetBrowserContext();
}

void AthenaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kTestType, "athena");
  // The NaCl sandbox won't work in our browser tests.
  command_line->AppendSwitch(switches::kNoSandbox);
  content::BrowserTestBase::SetUpCommandLine(command_line);
}

void AthenaBrowserTest::SetUpOnMainThread() {
  content::BrowserContext* context = GetBrowserContext();
  athena::StartAthenaEnv(content::BrowserThread::GetBlockingPool()
                             ->GetTaskRunnerWithShutdownBehavior(
                                 base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  athena::CreateVirtualKeyboardWithContext(context);
  athena::StartAthenaSessionWithContext(context);

  // Set the memory pressure to low and turning off undeterministic resource
  // observer events.
  test_util::SendTestMemoryPressureEvent(ResourceManager::MEMORY_PRESSURE_LOW);
}

void AthenaBrowserTest::RunTestOnMainThreadLoop() {
  base::MessageLoopForUI::current()->RunUntilIdle();
  SetUpOnMainThread();
  RunTestOnMainThread();
  TearDownOnMainThread();
}

}  // namespace test
}  // namespace athena
