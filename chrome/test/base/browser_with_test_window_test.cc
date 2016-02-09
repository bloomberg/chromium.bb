// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_with_test_window_test.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_CHROMEOS)
#include "ash/test/ash_test_helper.h"
#elif defined(TOOLKIT_VIEWS)
#include "ui/views/test/scoped_views_test_helper.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "components/constrained_window/constrained_window_views.h"
#endif

using content::NavigationController;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using content::WebContents;

BrowserWithTestWindowTest::BrowserWithTestWindowTest()
    : browser_type_(Browser::TYPE_TABBED),
      host_desktop_type_(chrome::HOST_DESKTOP_TYPE_NATIVE),
      hosted_app_(false) {
}

BrowserWithTestWindowTest::BrowserWithTestWindowTest(
    Browser::Type browser_type,
    chrome::HostDesktopType host_desktop_type,
    bool hosted_app)
    : browser_type_(browser_type),
      host_desktop_type_(host_desktop_type),
      hosted_app_(hosted_app) {
}

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {
}

void BrowserWithTestWindowTest::SetUp() {
  testing::Test::SetUp();
#if defined(OS_CHROMEOS)
  // TODO(jamescook): Windows Ash support. This will require refactoring
  // AshTestHelper and AuraTestHelper so they can be used at the same time,
  // perhaps by AshTestHelper owning an AuraTestHelper.
  ash_test_helper_.reset(new ash::test::AshTestHelper(
      base::MessageLoopForUI::current()));
  ash_test_helper_->SetUp(true);
#elif defined(TOOLKIT_VIEWS)
  views_test_helper_.reset(new views::ScopedViewsTestHelper());
#endif
#if defined(TOOLKIT_VIEWS)
  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());
#endif

  // Subclasses can provide their own Profile.
  profile_ = CreateProfile();
  // Subclasses can provide their own test BrowserWindow. If they return NULL
  // then Browser will create the a production BrowserWindow and the subclass
  // is responsible for cleaning it up (usually by NativeWidget destruction).
  window_.reset(CreateBrowserWindow());

  browser_.reset(CreateBrowser(profile(), browser_type_, hosted_app_,
                               host_desktop_type_, window_.get()));
}

void BrowserWithTestWindowTest::TearDown() {
  // Some tests end up posting tasks to the DB thread that must be completed
  // before the profile can be destroyed and the test safely shut down.
  base::RunLoop().RunUntilIdle();

  // Reset the profile here because some profile keyed services (like the
  // audio service) depend on test stubs that the helpers below will remove.
  DestroyBrowserAndProfile();

#if defined(TOOLKIT_VIEWS)
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
#endif

#if defined(OS_CHROMEOS)
  ash_test_helper_->TearDown();
#elif defined(TOOLKIT_VIEWS)
  views_test_helper_.reset();
#endif

  testing::Test::TearDown();

  // A Task is leaked if we don't destroy everything, then run the message
  // loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  base::MessageLoop::current()->Run();
}

gfx::NativeWindow BrowserWithTestWindowTest::GetContext() {
#if defined(OS_CHROMEOS)
  return ash_test_helper_->CurrentContext();
#elif defined(TOOLKIT_VIEWS)
  return views_test_helper_->GetContext();
#else
  return nullptr;
#endif
}

void BrowserWithTestWindowTest::AddTab(Browser* browser, const GURL& url) {
  chrome::NavigateParams params(browser, url, ui::PAGE_TRANSITION_TYPED);
  params.tabstrip_index = 0;
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  CommitPendingLoad(&params.target_contents->GetController());
}

void BrowserWithTestWindowTest::CommitPendingLoad(
  NavigationController* controller) {
  if (!controller->GetPendingEntry())
    return;  // Nothing to commit.

  RenderFrameHost* old_rfh = controller->GetWebContents()->GetMainFrame();

  RenderFrameHost* pending_rfh = RenderFrameHostTester::GetPendingForController(
      controller);
  if (pending_rfh) {
    // Simulate the BeforeUnload_ACK that is received from the current renderer
    // for a cross-site navigation.
    DCHECK_NE(old_rfh, pending_rfh);
    RenderFrameHostTester::For(old_rfh)->SendBeforeUnloadACK(true);
  }
  // Commit on the pending_rfh, if one exists.
  RenderFrameHost* test_rfh = pending_rfh ? pending_rfh : old_rfh;
  RenderFrameHostTester* test_rfh_tester = RenderFrameHostTester::For(test_rfh);

  // Simulate a SwapOut_ACK before the navigation commits.
  if (pending_rfh)
    RenderFrameHostTester::For(old_rfh)->SimulateSwapOutACK();

  // For new navigations, we need to send a larger page ID. For renavigations,
  // we need to send the preexisting page ID. We can tell these apart because
  // renavigations will have a pending_entry_index while new ones won't (they'll
  // just have a standalong pending_entry that isn't in the list already).
  if (controller->GetPendingEntryIndex() >= 0) {
    test_rfh_tester->SendNavigateWithTransition(
        controller->GetPendingEntry()->GetPageID(),
        controller->GetPendingEntry()->GetUniqueID(),
        false,
        controller->GetPendingEntry()->GetURL(),
        controller->GetPendingEntry()->GetTransitionType());
  } else {
    test_rfh_tester->SendNavigateWithTransition(
        controller->GetWebContents()->GetMaxPageIDForSiteInstance(
            test_rfh->GetSiteInstance()) + 1,
        controller->GetPendingEntry()->GetUniqueID(),
        true,
        controller->GetPendingEntry()->GetURL(),
        controller->GetPendingEntry()->GetTransitionType());
  }
}

void BrowserWithTestWindowTest::NavigateAndCommit(
    NavigationController* controller,
    const GURL& url) {
  controller->LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
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
    const base::string16& title) {
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
  if (profile_)
    DestroyProfile(profile_);
  profile_ = NULL;
}

TestingProfile* BrowserWithTestWindowTest::CreateProfile() {
  return new TestingProfile();
}

void BrowserWithTestWindowTest::DestroyProfile(TestingProfile* profile) {
  delete profile;
}

BrowserWindow* BrowserWithTestWindowTest::CreateBrowserWindow() {
  return new TestBrowserWindow();
}

Browser* BrowserWithTestWindowTest::CreateBrowser(
    Profile* profile,
    Browser::Type browser_type,
    bool hosted_app,
    chrome::HostDesktopType host_desktop_type,
    BrowserWindow* browser_window) {
  Browser::CreateParams params(profile);
  if (hosted_app) {
    params = Browser::CreateParams::CreateForApp(
        "Test", true /* trusted_source */, gfx::Rect(), profile);
  } else {
    params.type = browser_type;
  }
  params.window = browser_window;
  return new Browser(params);
}
