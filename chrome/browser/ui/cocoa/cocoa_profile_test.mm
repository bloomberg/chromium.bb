// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"

using content::BrowserThread;

CocoaProfileTest::CocoaProfileTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      profile_(NULL),
      file_user_blocking_thread_(new content::TestBrowserThread(
          BrowserThread::FILE_USER_BLOCKING, &message_loop_)),
      file_thread_(new content::TestBrowserThread(BrowserThread::FILE,
                                                  &message_loop_)),
      io_thread_(new content::TestBrowserThread(BrowserThread::IO,
                                                &message_loop_)) {
}

CocoaProfileTest::~CocoaProfileTest() {
  // Delete the testing profile on the UI thread. But first release the
  // browser, since it may trigger accesses to the profile upon destruction.
  browser_.reset();

  message_loop_.RunUntilIdle();
  // Some services created on the TestingProfile require deletion on the UI
  // thread. If the scoper in TestingBrowserProcess, owned by ChromeTestSuite,
  // were to delete the ProfileManager, the UI thread would at that point no
  // longer exist.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(
      NULL);

  // Make sure any pending tasks run before we destroy other threads.
  message_loop_.RunUntilIdle();

  // Drop any new tasks for the IO and FILE threads.
  io_thread_.reset();
  file_user_blocking_thread_.reset();
  file_thread_.reset();

  message_loop_.RunUntilIdle();
}

void CocoaProfileTest::SetUp() {
  CocoaTest::SetUp();

  ASSERT_TRUE(profile_manager_.SetUp());

  profile_ = profile_manager_.CreateTestingProfile("default");
  ASSERT_TRUE(profile_);

  profile_->CreateBookmarkModel(true);
  profile_->BlockUntilBookmarkModelLoaded();

  // TODO(shess): These are needed in case someone creates a browser
  // window off of browser_.  pkasting indicates that other
  // platforms use a stub |BrowserWindow| and thus don't need to do
  // this.
  // http://crbug.com/39725
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_, &TemplateURLServiceFactory::BuildInstanceFor);
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_, &AutocompleteClassifierFactory::BuildInstanceFor);

  browser_.reset(CreateBrowser());
  ASSERT_TRUE(browser_.get());
}

void CocoaProfileTest::TearDown() {
  if (browser_.get() && browser_->window())
    CloseBrowserWindow();

  CocoaTest::TearDown();
}

void CocoaProfileTest::CloseBrowserWindow() {
  // Check to make sure a window was actually created.
  DCHECK(browser_->window());
  browser_->tab_strip_model()->CloseAllTabs();
  chrome::CloseWindow(browser_.get());
  // |browser_| will be deleted by its BrowserWindowController.
  ignore_result(browser_.release());
}

Browser* CocoaProfileTest::CreateBrowser() {
  return new Browser(Browser::CreateParams(profile()));
}
