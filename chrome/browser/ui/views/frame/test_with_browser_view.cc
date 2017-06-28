// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/test_with_browser_view.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_io_thread_state.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#endif

namespace {

std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return base::MakeUnique<TemplateURLService>(
      profile->GetPrefs(),
      std::unique_ptr<SearchTermsData>(new UIThreadSearchTermsData(profile)),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::unique_ptr<TemplateURLServiceClient>(
          new ChromeTemplateURLServiceClient(
              HistoryServiceFactory::GetForProfile(
                  profile, ServiceAccessType::EXPLICIT_ACCESS))),
      nullptr, nullptr, base::Closure());
}

std::unique_ptr<KeyedService> CreateAutocompleteClassifier(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return base::MakeUnique<AutocompleteClassifier>(
      base::WrapUnique(new AutocompleteController(
          base::WrapUnique(new ChromeAutocompleteProviderClient(profile)),

          nullptr, AutocompleteClassifier::DefaultOmniboxProviders())),
      std::unique_ptr<AutocompleteSchemeClassifier>(
          new TestSchemeClassifier()));
}

}  // namespace

TestWithBrowserView::TestWithBrowserView() {
}

TestWithBrowserView::TestWithBrowserView(Browser::Type browser_type,
                                         bool hosted_app)
    : BrowserWithTestWindowTest(browser_type, hosted_app) {}

TestWithBrowserView::~TestWithBrowserView() {
}

void TestWithBrowserView::SetUp() {
  local_state_.reset(
      new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));
#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManagerImpl);
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
  browser_view_ = nullptr;
  content::RunAllPendingInMessageLoop(content::BrowserThread::DB);
  BrowserWithTestWindowTest::TearDown();
  testing_io_thread_state_.reset();
  predictor_db_.reset();
#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
  local_state_.reset(nullptr);
}

TestingProfile* TestWithBrowserView::CreateProfile() {
  TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
  // TemplateURLService is normally null during testing. Instant extended
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
  return nullptr;
}
