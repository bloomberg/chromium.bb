// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "content/test/test_content_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(USE_X11)
#include "ui/base/ime/input_method_initializer.h"
#endif

namespace content {

ContentBrowserTest::ContentBrowserTest()
    : setup_called_(false) {
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
  base::FilePath content_test_data_dir;
  CHECK(PathService::Get(DIR_TEST_DATA, &content_test_data_dir));
  embedded_test_server()->ServeFilesFromDirectory(content_test_data_dir);
}

ContentBrowserTest::~ContentBrowserTest() {
  CHECK(setup_called_) << "Overridden SetUp() did not call parent "
                          "implementation, so test not run.";
}

void ContentBrowserTest::SetUp() {
  setup_called_ = true;

  shell_main_delegate_.reset(new ShellMainDelegate);
  shell_main_delegate_->PreSandboxStartup();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kContentBrowserTest);

  SetUpCommandLine(command_line);

  // Single-process mode is not set in BrowserMain, so process it explicitly,
  // and set up renderer.
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    single_process_renderer_client_.reset(new ShellContentRendererClient);
    SetRendererClientForTesting(single_process_renderer_client_.get());
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

  // LinuxInputMethodContextFactory has to be initialized.
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(USE_X11)
  ui::InitializeInputMethodForTesting();
#endif

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  // LinuxInputMethodContextFactory has to be shutdown.
#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(USE_X11)
  ui::ShutdownInputMethodForTesting();
#endif

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
  base::MessageLoopForUI::current()->RunUntilIdle();

#if defined(OS_MACOSX)
  pool.Recycle();
#endif

  SetUpOnMainThread();

  RunTestOnMainThread();

  TearDownOnMainThread();
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
  return Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->browser_context(),
      GURL(kAboutBlankURL),
      NULL,
      MSG_ROUTING_NONE,
      gfx::Size());
}

Shell* ContentBrowserTest::CreateOffTheRecordBrowser() {
  return Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->off_the_record_browser_context(),
      GURL(kAboutBlankURL),
      NULL,
      MSG_ROUTING_NONE,
      gfx::Size());
}

}  // namespace content
