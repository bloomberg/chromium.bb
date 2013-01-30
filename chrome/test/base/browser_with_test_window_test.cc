// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_with_test_window_test.h"

#include "base/synchronization/waitable_event.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_renderer_host.h"

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::RenderViewHost;
using content::RenderViewHostTester;
using content::WebContents;

BrowserWithTestWindowTest::BrowserWithTestWindowTest()
    : ui_thread_(BrowserThread::UI, message_loop()),
      db_thread_(BrowserThread::DB),
      file_thread_(BrowserThread::FILE, message_loop()),
      file_user_blocking_thread_(
          BrowserThread::FILE_USER_BLOCKING, message_loop()),
      host_desktop_type_(chrome::HOST_DESKTOP_TYPE_NATIVE) {
  db_thread_.Start();
}

BrowserWithTestWindowTest::BrowserWithTestWindowTest(
    chrome::HostDesktopType host_desktop_type)
    : ui_thread_(BrowserThread::UI, message_loop()),
      db_thread_(BrowserThread::DB),
      file_thread_(BrowserThread::FILE, message_loop()),
      file_user_blocking_thread_(
          BrowserThread::FILE_USER_BLOCKING, message_loop()),
      host_desktop_type_(host_desktop_type) {
  db_thread_.Start();
}

void BrowserWithTestWindowTest::SetUp() {
  testing::Test::SetUp();

  set_profile(CreateProfile());

  // Allow subclasses to specify a |window_| in their SetUp().
  if (!window_.get())
    window_.reset(new TestBrowserWindow);

  Browser::CreateParams params(profile(), host_desktop_type_);
  params.window = window_.get();
  browser_.reset(new Browser(params));
#if defined(USE_AURA)
  aura_test_helper_.reset(new aura::test::AuraTestHelper(&ui_loop_));
  aura_test_helper_->SetUp();
#endif  // USE_AURA
}

void BrowserWithTestWindowTest::TearDown() {
  testing::Test::TearDown();
#if defined(USE_AURA)
  aura_test_helper_->TearDown();
#endif
}

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {
  // A Task is leaked if we don't destroy everything, then run the message
  // loop.
  DestroyBrowserAndProfile();

  // Schedule another task on the DB thread to notify us that it's safe to
  // carry on with the test.
  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();
  db_thread_.Stop();
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
}

void BrowserWithTestWindowTest::set_profile(TestingProfile* profile) {
  if (profile_.get() != NULL)
    ProfileDestroyer::DestroyProfileWhenAppropriate(profile_.release());

  profile_.reset(profile);
}

void BrowserWithTestWindowTest::AddTab(Browser* browser, const GURL& url) {
  chrome::NavigateParams params(browser, url, content::PAGE_TRANSITION_TYPED);
  params.tabstrip_index = 0;
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  CommitPendingLoad(&params.target_contents->GetController());
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
  NavigateAndCommit(&browser()->tab_strip_model()->GetActiveWebContents()->
                        GetController(),
                    url);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTabWithTitle(
    Browser* navigating_browser,
    const GURL& url,
    const string16& title) {
  NavigationController* controller = &navigating_browser->tab_strip_model()->
      GetActiveWebContents()->GetController();
  NavigateAndCommit(controller, url);
  controller->GetActiveEntry()->SetTitle(title);
}

void BrowserWithTestWindowTest::DestroyBrowserAndProfile() {
  if (browser_.get()) {
    // Make sure we close all tabs, otherwise Browser isn't happy in its
    // destructor.
    browser()->tab_strip_model()->CloseAllTabs();
    browser_.reset(NULL);
  }
  window_.reset(NULL);
  // Destroy the profile here - otherwise, if the profile is freed in the
  // destructor, and a test subclass owns a resource that the profile depends
  // on (such as g_browser_process()->local_state()) there's no way for the
  // subclass to free it after the profile.
  profile_.reset(NULL);
}

TestingProfile* BrowserWithTestWindowTest::CreateProfile() {
  return new TestingProfile();
}
