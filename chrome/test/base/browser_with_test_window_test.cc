// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_with_test_window_test.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif  // defined(OS_WIN)

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/page_transition_types.h"

using content::BrowserThread;

BrowserWithTestWindowTest::BrowserWithTestWindowTest()
    : ui_thread_(BrowserThread::UI, message_loop()),
      file_thread_(BrowserThread::FILE, message_loop()),
      rph_factory_(),
      rvh_factory_(&rph_factory_) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif
}

void BrowserWithTestWindowTest::SetUp() {
  testing::Test::SetUp();

  profile_.reset(CreateProfile());
  browser_.reset(new Browser(Browser::TYPE_TABBED, profile()));
  window_.reset(new TestBrowserWindow(browser()));
  browser_->set_window(window_.get());
}

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {
  // A Task is leaked if we don't destroy everything, then run the message
  // loop.
  DestroyBrowser();
  profile_.reset(NULL);

  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();

#if defined(OS_WIN)
  OleUninitialize();
#endif
}

TestRenderViewHost* BrowserWithTestWindowTest::TestRenderViewHostForTab(
    TabContents* tab_contents) {
  return static_cast<TestRenderViewHost*>(
      tab_contents->render_view_host());
}

void BrowserWithTestWindowTest::AddTab(Browser* browser, const GURL& url) {
  browser::NavigateParams params(browser, url, content::PAGE_TRANSITION_TYPED);
  params.tabstrip_index = 0;
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  CommitPendingLoad(&params.target_contents->controller());
}

void BrowserWithTestWindowTest::CommitPendingLoad(
    NavigationController* controller) {
  if (!controller->pending_entry())
    return;  // Nothing to commit.

  TestRenderViewHost* old_rvh =
      TestRenderViewHostForTab(controller->tab_contents());
  MockRenderProcessHost* mock_rph = static_cast<MockRenderProcessHost*>(
    old_rvh->process());

  TestRenderViewHost* pending_rvh = static_cast<TestRenderViewHost*>(
      controller->tab_contents()->render_manager_for_testing()->
      pending_render_view_host());
  if (pending_rvh) {
    // Simulate the ShouldClose_ACK that is received from the current renderer
    // for a cross-site navigation.
    DCHECK_NE(old_rvh, pending_rvh);
    old_rvh->SendShouldCloseACK(true);
  }
  // Commit on the pending_rvh, if one exists.
  TestRenderViewHost* test_rvh = pending_rvh ? pending_rvh : old_rvh;

  // For new navigations, we need to send a larger page ID. For renavigations,
  // we need to send the preexisting page ID. We can tell these apart because
  // renavigations will have a pending_entry_index while new ones won't (they'll
  // just have a standalong pending_entry that isn't in the list already).
  if (controller->pending_entry_index() >= 0) {
    test_rvh->SendNavigateWithTransition(
        controller->pending_entry()->page_id(),
        controller->pending_entry()->url(),
        controller->pending_entry()->transition_type());
  } else {
    test_rvh->SendNavigateWithTransition(
        mock_rph->max_page_id() + 1,
        controller->pending_entry()->url(),
        controller->pending_entry()->transition_type());
    mock_rph->UpdateMaxPageID(mock_rph->max_page_id() + 1);
  }

  // Simulate the SwapOut_ACK that fires if you commit a cross-site navigation
  // without making any network requests.
  if (pending_rvh)
    old_rvh->OnSwapOutACK();
}

void BrowserWithTestWindowTest::NavigateAndCommit(
    NavigationController* controller,
    const GURL& url) {
  controller->LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  CommitPendingLoad(controller);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTab(const GURL& url) {
  NavigateAndCommit(&browser()->GetSelectedTabContents()->controller(), url);
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
