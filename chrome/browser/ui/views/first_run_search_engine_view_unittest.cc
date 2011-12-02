// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_search_engine_view.h"

#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
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
