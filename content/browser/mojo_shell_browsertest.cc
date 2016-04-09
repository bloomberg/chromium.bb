// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/mojo/mojo_shell_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/mojo_app_connection.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_mojo_app.h"
#include "content/public/test/test_mojo_service.mojom.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {

const char kInProcessTestMojoAppName[] = "system:content_in_process_test_app";

class MojoShellTest : public ContentBrowserTest {
 public:
  MojoShellTest() {
    test_apps_[kInProcessTestMojoAppName] = base::Bind(&CreateTestApp);
    MojoShellContext::SetApplicationsForTest(&test_apps_);
  }

 protected:
  std::string GetUserId() {
    return BrowserContext::GetMojoUserIdFor(
        shell()->web_contents()->GetBrowserContext());
  }

 private:
  static std::unique_ptr<mojo::ShellClient> CreateTestApp() {
    return std::unique_ptr<mojo::ShellClient>(new TestMojoApp);
  }

  MojoShellContext::StaticApplicationMap test_apps_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellTest);
};

IN_PROC_BROWSER_TEST_F(MojoShellTest, TestBrowserConnection) {
  auto test_app = MojoAppConnection::Create(
      GetUserId(), kInProcessTestMojoAppName, kBrowserMojoAppUrl);
  mojom::TestMojoServicePtr test_service;
  test_app->GetInterface(&test_service);

  base::RunLoop run_loop;
  test_service->DoSomething(run_loop.QuitClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(MojoShellTest, TestUtilityConnection) {
  // With no loader registered at this URL, the shell should spawn a utility
  // process and connect us to it. content_shell's utility process always hosts
  // a TestMojoApp at |kTestMojoAppUrl|.
  auto test_app = MojoAppConnection::Create(GetUserId(), kTestMojoAppUrl,
                                            kBrowserMojoAppUrl);
  mojom::TestMojoServicePtr test_service;
  test_app->GetInterface(&test_service);

  base::RunLoop run_loop;
  test_service->DoSomething(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content
