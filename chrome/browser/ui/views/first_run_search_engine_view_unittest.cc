// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_search_engine_view.h"

#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

typedef views::ViewsTestBase FirstRunSearchEngineViewTest;

TEST_F(FirstRunSearchEngineViewTest, ClosingSelectsFirstEngine) {
  // Create the first run search engine selector, and just close the window.
  // The first engine in the vector returned by GetTemplateURLs should be set as
  // the default engine.
  TestingProfile profile;
  profile.CreateTemplateURLService();
  profile.BlockUntilTemplateURLServiceLoaded();

  // Set a dummy provider as the default so we can verify something changed.
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service != NULL);
  EXPECT_EQ(NULL, service->GetDefaultSearchProvider());
  TemplateURL* d1 = new TemplateURL;
  TemplateURL* d2 = new TemplateURL;
  TemplateURL* d3 = new TemplateURL;
  service->Add(d1);
  service->Add(d2);
  service->Add(d3);
  service->SetDefaultSearchProvider(d3);

  FirstRunSearchEngineView* contents =
      new FirstRunSearchEngineView(&profile, false);
  contents->set_quit_on_closing(false);
  views::Widget* window = views::Widget::CreateWindow(contents);
  window->Show();
  window->Close();
  RunPendingMessages();  // Allows the window to be destroyed after Close();

  TemplateURLService::TemplateURLVector template_urls =
      service->GetTemplateURLs();
  ASSERT_TRUE(!template_urls.empty());
  EXPECT_EQ(template_urls.front(), service->GetDefaultSearchProvider());
}

TEST_F(FirstRunSearchEngineViewTest, ClosingBeforeServiceLoadedAbortsClose) {
  // This ensures the current thread is named the UI thread, so code that checks
  // that this is the UI thread doesn't assert.
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       message_loop());
  content::BrowserProcessSubThread db_thread(content::BrowserThread::DB);
  db_thread.StartWithOptions(base::Thread::Options());

  TestingProfile profile;
  // We need to initialize the web database before accessing the template url
  // service otherwise the template url service will init itself synchronously
  // and appear to be loaded.
  profile.CreateWebDataService(false);
  profile.CreateTemplateURLService();

  // Instead of giving the template url service a chance to load, try and close
  // the window immediately.
  FirstRunSearchEngineView* contents =
      new FirstRunSearchEngineView(&profile, false);
  contents->set_quit_on_closing(false);
  views::Widget* window = views::Widget::CreateWindow(contents);
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  window->Close();
  // The window wouldn't actually be closed until a return to the message loop,
  // which we don't want to spin here because the window shouldn't have closed
  // in the correct case. The window is however actually hidden immediately when
  // the window is allowed to close, so we can test for visibility to make sure
  // it hasn't.
  EXPECT_TRUE(window->IsVisible());

  // Now let the template url service a chance to load.
  ui_test_utils::WindowedNotificationObserver service_load_observer(
      chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
      content::NotificationService::AllSources());
  service_load_observer.Wait();

  // .. and try and close the window. It should be allowed to now.
  window->Close();
  EXPECT_FALSE(window->IsVisible());

  // Allow the window to actually close.
  RunPendingMessages();

  // Verify goodness. Because we actually went to the trouble of starting the
  // WebDB, we will have real data in the model, so we can verify a choice was
  // made without having to seed the model with dummy data.
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service != NULL);
  EXPECT_TRUE(service->GetDefaultSearchProvider() != NULL);
}
