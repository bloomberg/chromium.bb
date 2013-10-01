// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/test_with_browser_view.h"

#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_io_thread_state.h"
#include "content/public/test/test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#endif

namespace {

// Caller owns the returned service.
BrowserContextKeyedService* CreateTemplateURLService(
    content::BrowserContext* profile) {
  return new TemplateURLService(static_cast<Profile*>(profile));
}

BrowserContextKeyedService* CreateAutocompleteClassifier(
    content::BrowserContext* profile) {
  return new AutocompleteClassifier(static_cast<Profile*>(profile));
}

}  // namespace

TestWithBrowserView::TestWithBrowserView() {
}

TestWithBrowserView::~TestWithBrowserView() {
}

void TestWithBrowserView::SetUp() {
  local_state_.reset(
      new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));
#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManager);
#endif
  testing_io_thread_state_.reset(new chrome::TestingIOThreadState());
  BrowserWithTestWindowTest::SetUp();
  predictor_db_.reset(new predictors::PredictorDatabase(GetProfile()));
  browser_view_ = static_cast<BrowserView*>(browser()->window());
}

void TestWithBrowserView::TearDown() {
  // Both BrowserView and BrowserWithTestWindowTest believe they have ownership
  // of the Browser. Force BrowserWithTestWindowTest to give up ownership.
  ASSERT_TRUE(release_browser());

  // Clean up any tabs we opened, otherwise Browser crashes in destruction.
  browser_view_->browser()->tab_strip_model()->CloseAllTabs();
  // Ensure the Browser is reset before BrowserWithTestWindowTest cleans up
  // the Profile.
  browser_view_->GetWidget()->CloseNow();
  browser_view_ = NULL;
  content::RunAllPendingInMessageLoop(content::BrowserThread::DB);
  BrowserWithTestWindowTest::TearDown();
  testing_io_thread_state_.reset();
  predictor_db_.reset();
#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
  local_state_.reset(NULL);
}

TestingProfile* TestWithBrowserView::CreateProfile() {
  TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
  // TemplateURLService is normally NULL during testing. Instant extended
  // needs this service so set a custom factory function.
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile, &CreateTemplateURLService);
  // TODO(jamescook): Eliminate this by introducing a mock toolbar or mock
  // location bar.
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactory(
      profile, &CreateAutocompleteClassifier);
  return profile;
}

BrowserWindow* TestWithBrowserView::CreateBrowserWindow() {
  // Allow BrowserWithTestWindowTest to use Browser to create the default
  // BrowserView and BrowserFrame.
  return NULL;
}
