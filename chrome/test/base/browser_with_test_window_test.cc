// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_with_test_window_test.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/test_renderer_host.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/gfx/screen.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::RenderViewHost;
using content::RenderViewHostTester;
using content::WebContents;

BrowserWithTestWindowTest::BrowserWithTestWindowTest()
    : ui_thread_(BrowserThread::UI, message_loop()),
      file_thread_(BrowserThread::FILE, message_loop()),
      file_user_blocking_thread_(
          BrowserThread::FILE_USER_BLOCKING, message_loop()) {
}

void BrowserWithTestWindowTest::SetUp() {
  testing::Test::SetUp();

  profile_.reset(CreateProfile());
  browser_.reset(new Browser(Browser::TYPE_TABBED, profile()));
  window_.reset(new TestBrowserWindow(browser()));
  browser_->SetWindowForTesting(window_.get());
#if defined(USE_AURA)
  aura::Env::GetInstance()->SetMonitorManager(new aura::SingleMonitorManager);
  root_window_.reset(aura::MonitorManager::CreateRootWindowForPrimaryMonitor());
  gfx::Screen::SetInstance(new aura::TestScreen(root_window_.get()));
  test_activation_client_.reset(
      new aura::test::TestActivationClient(root_window_.get()));
  test_stacking_client_.reset(
      new aura::test::TestStackingClient(root_window_.get()));
#endif  // USE_AURA
}

void BrowserWithTestWindowTest::TearDown() {
  testing::Test::TearDown();
#if defined(USE_AURA)
  test_activation_client_.reset();
  test_stacking_client_.reset();
  root_window_.reset();
#endif
}

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {
  // A Task is leaked if we don't destroy everything, then run the message
  // loop.
  DestroyBrowser();
  profile_.reset(NULL);

  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
}

void BrowserWithTestWindowTest::AddTab(Browser* browser, const GURL& url) {
  browser::NavigateParams params(browser, url, content::PAGE_TRANSITION_TYPED);
  params.tabstrip_index = 0;
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  CommitPendingLoad(&params.target_contents->web_contents()->GetController());
}

void BrowserWithTestWindowTest::CommitPendingLoad(
  NavigationController* controller) {
  if (!controller->GetPendingEntry())
    return;  // Nothing to commit.

  RenderViewHost* old_rvh =
      controller->GetWebContents()->GetRenderViewHost();

  RenderViewHost* pending_rvh = RenderViewHostTester::GetPendingForController(
      controller);
  if (pending_rvh) {
    // Simulate the ShouldClose_ACK that is received from the current renderer
    // for a cross-site navigation.
    DCHECK_NE(old_rvh, pending_rvh);
    RenderViewHostTester::For(old_rvh)->SendShouldCloseACK(true);
  }
  // Commit on the pending_rvh, if one exists.
  RenderViewHost* test_rvh = pending_rvh ? pending_rvh : old_rvh;
  RenderViewHostTester* test_rvh_tester = RenderViewHostTester::For(test_rvh);

  // For new navigations, we need to send a larger page ID. For renavigations,
  // we need to send the preexisting page ID. We can tell these apart because
  // renavigations will have a pending_entry_index while new ones won't (they'll
  // just have a standalong pending_entry that isn't in the list already).
  if (controller->GetPendingEntryIndex() >= 0) {
    test_rvh_tester->SendNavigateWithTransition(
        controller->GetPendingEntry()->GetPageID(),
        controller->GetPendingEntry()->GetURL(),
        controller->GetPendingEntry()->GetTransitionType());
  } else {
    test_rvh_tester->SendNavigateWithTransition(
        controller->GetWebContents()->
            GetMaxPageIDForSiteInstance(test_rvh->GetSiteInstance()) + 1,
        controller->GetPendingEntry()->GetURL(),
        controller->GetPendingEntry()->GetTransitionType());
  }

  if (pending_rvh)
    RenderViewHostTester::For(old_rvh)->SimulateSwapOutACK();
}

void BrowserWithTestWindowTest::NavigateAndCommit(
    NavigationController* controller,
    const GURL& url) {
  controller->LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  CommitPendingLoad(controller);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTab(const GURL& url) {
  NavigateAndCommit(&browser()->GetSelectedTabContentsWrapper()->
      web_contents()->GetController(), url);
}

void BrowserWithTestWindowTest::DestroyBrowser() {
  if (!browser_.get())
    return;
  // Make sure we close all tabs, otherwise Browser isn't happy in its
  // destructor.
  browser()->CloseAllTabs();
  browser_.reset(NULL);
  window_.reset(NULL);
}

TestingProfile* BrowserWithTestWindowTest::CreateProfile() {
  return new TestingProfile();
}
