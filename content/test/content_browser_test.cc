// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_renderer_client.h"
#include "content/shell/shell_main_delegate.h"
#include "content/shell/shell_switches.h"
#include "content/test/test_content_client.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace content {

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, MANUAL_ShouldntRun) {
  // Ensures that tests with MANUAL_ prefix don't run automatically.
  ASSERT_TRUE(false);
}

ContentBrowserTest::ContentBrowserTest() {
#if defined(OS_MACOSX)
  // See comment in InProcessBrowserTest::InProcessBrowserTest().
  base::FilePath content_shell_path;
  CHECK(PathService::Get(base::FILE_EXE, &content_shell_path));
  content_shell_path = content_shell_path.DirName();
  content_shell_path = content_shell_path.Append(
      FILE_PATH_LITERAL("Content Shell.app/Contents/MacOS/Content Shell"));
  CHECK(PathService::Override(base::FILE_EXE, content_shell_path));
#endif
  CreateTestServer(base::FilePath(FILE_PATH_LITERAL("content/test/data")));
}

ContentBrowserTest::~ContentBrowserTest() {
}

void ContentBrowserTest::SetUp() {
  shell_main_delegate_.reset(new ShellMainDelegate);
  shell_main_delegate_->PreSandboxStartup();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kContentBrowserTest);

  SetUpCommandLine(command_line);

  // Single-process mode is not set in BrowserMain, so process it explicitly,
  // and set up renderer.
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    single_process_renderer_client_.reset(new ShellContentRendererClient);
    GetContentClient()->set_renderer_for_testing(
        single_process_renderer_client_.get());
  }

#if defined(OS_MACOSX)
  // See InProcessBrowserTest::PrepareTestCommandLine().
  base::FilePath subprocess_path;
  PathService::Get(base::FILE_EXE, &subprocess_path);
  subprocess_path = subprocess_path.DirName().DirName();
  DCHECK_EQ(subprocess_path.BaseName().value(), "Contents");
  subprocess_path = subprocess_path.Append(
      "Frameworks/Content Shell Helper.app/Contents/MacOS/Content Shell Helper");
  command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                 subprocess_path);
#endif

  // NOTE: should be kept in sync with
  // chrome/browser/resources/software_rendering_list.json
#if !defined(OS_WIN) && !defined(OS_CHROMEOS)
  command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
#endif

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  shell_main_delegate_.reset();
}

void ContentBrowserTest::RunTestOnMainThreadLoop() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    CHECK_EQ(Shell::windows().size(), 1u);
    shell_ = Shell::windows()[0];
  }

#if defined(OS_MACOSX)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  // Pump startup related events.
  MessageLoopForUI::current()->RunUntilIdle();

#if defined(OS_MACOSX)
  pool.Recycle();
#endif

  SetUpOnMainThread();

  RunTestOnMainThread();
#if defined(OS_MACOSX)
  pool.Recycle();
#endif

  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }

  Shell::CloseAllWindows();
}

Shell* ContentBrowserTest::CreateBrowser() {
  ShellContentBrowserClient* browser_client =
      static_cast<ShellContentBrowserClient*>(GetContentClient()->browser());
  return Shell::CreateNewWindow(
      browser_client->browser_context(),
      GURL(chrome::kAboutBlankURL),
      NULL,
      MSG_ROUTING_NONE,
      gfx::Size());
}

Shell* ContentBrowserTest::CreateOffTheRecordBrowser() {
  ShellContentBrowserClient* browser_client =
      static_cast<ShellContentBrowserClient*>(GetContentClient()->browser());
  return Shell::CreateNewWindow(
      browser_client->off_the_record_browser_context(),
      GURL(chrome::kAboutBlankURL),
      NULL,
      MSG_ROUTING_NONE,
      gfx::Size());
}

}  // namespace content
