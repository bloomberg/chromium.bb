// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_base_aura.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/test/shell_test_extensions_browser_client.h"
#include "extensions/shell/test/shell_test_helper_aura.h"
#include "url/gurl.h"

namespace extensions {

ShellTestBaseAura::ShellTestBaseAura()
    : ExtensionsTest(base::MakeUnique<content::TestBrowserThreadBundle>()) {}

ShellTestBaseAura::~ShellTestBaseAura() {}

void ShellTestBaseAura::SetUp() {
  helper_ = base::MakeUnique<ShellTestHelperAura>();
  helper_->SetUp();
  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client =
      base::MakeUnique<ShellTestExtensionsBrowserClient>();
  SetExtensionsBrowserClient(std::move(extensions_browser_client));
  ExtensionsTest::SetUp();
}

void ShellTestBaseAura::TearDown() {
  ExtensionsTest::TearDown();
  helper_->TearDown();
}

void ShellTestBaseAura::InitAppWindow(AppWindow* app_window,
                                      const gfx::Rect& bounds) {
  // Create a TestAppWindowContents for the ShellAppDelegate to initialize the
  // ShellExtensionWebContentsObserver with.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser_context())));
  std::unique_ptr<TestAppWindowContents> app_window_contents =
      base::MakeUnique<TestAppWindowContents>(std::move(web_contents));

  // Initialize the web contents and AppWindow.
  app_window->app_delegate()->InitWebContents(
      app_window_contents->GetWebContents());

  content::RenderFrameHost* main_frame =
      app_window_contents->GetWebContents()->GetMainFrame();
  DCHECK(main_frame);

  AppWindow::CreateParams params;
  params.content_spec.bounds = bounds;
  app_window->Init(GURL(), app_window_contents.release(), main_frame, params);
}

}  // namespace extensions
