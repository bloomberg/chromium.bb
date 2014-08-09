// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_unit_test_suite.h"

#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/omaha_query_params/chrome_omaha_query_params_delegate.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_paths.h"
#endif

#if !defined(OS_IOS)
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "extensions/common/extension_paths.h"
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_POSIX)
#include "base/memory/shared_memory.h"
#endif

namespace {

// Creates a TestingBrowserProcess for each test.
class ChromeUnitTestSuiteInitializer : public testing::EmptyTestEventListener {
 public:
  ChromeUnitTestSuiteInitializer() {}
  virtual ~ChromeUnitTestSuiteInitializer() {}

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    content_client_.reset(new ChromeContentClient);
    content::SetContentClient(content_client_.get());
    // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
    browser_content_client_.reset(new chrome::ChromeContentBrowserClient());
    content::SetBrowserClientForTesting(browser_content_client_.get());
    utility_content_client_.reset(new ChromeContentUtilityClient());
    content::SetUtilityClientForTesting(utility_content_client_.get());
#endif

    TestingBrowserProcess::CreateInstance();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
    browser_content_client_.reset();
    utility_content_client_.reset();
#endif
    content_client_.reset();
    content::SetContentClient(NULL);

    TestingBrowserProcess::DeleteInstance();
  }

 private:
  // Client implementations for the content module.
  scoped_ptr<ChromeContentClient> content_client_;
  // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
  scoped_ptr<chrome::ChromeContentBrowserClient> browser_content_client_;
  scoped_ptr<ChromeContentUtilityClient> utility_content_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeUnitTestSuiteInitializer);
};

}  // namespace

ChromeUnitTestSuite::ChromeUnitTestSuite(int argc, char** argv)
    : ChromeTestSuite(argc, argv) {}

ChromeUnitTestSuite::~ChromeUnitTestSuite() {}

void ChromeUnitTestSuite::Initialize() {
  // Add an additional listener to do the extra initialization for unit tests.
  // It will be started before the base class listeners and ended after the
  // base class listeners.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ChromeUnitTestSuiteInitializer);

  InitializeProviders();
  RegisterInProcessThreads();

  ChromeTestSuite::Initialize();

  // This needs to run after ChromeTestSuite::Initialize which calls content's
  // intialization which calls base's which initializes ICU.
  InitializeResourceBundle();
}

void ChromeUnitTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();
  ChromeTestSuite::Shutdown();
}

void ChromeUnitTestSuite::InitializeProviders() {
  {
    ChromeContentClient content_client;
    RegisterContentSchemes(&content_client);
  }

  chrome::RegisterPathProvider();
  content::RegisterPathProvider();
  ui::RegisterPathProvider();
  component_updater::RegisterPathProvider(chrome::DIR_USER_DATA);

#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif

#if !defined(OS_IOS)
  extensions::RegisterPathProvider();

  extensions::ExtensionsClient::Set(
      extensions::ChromeExtensionsClient::GetInstance());

  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());

  gfx::GLSurface::InitializeOneOffForTests();

  omaha_query_params::OmahaQueryParams::SetDelegate(
      ChromeOmahaQueryParamsDelegate::GetInstance());
#endif
}

void ChromeUnitTestSuite::InitializeResourceBundle() {
  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  base::FilePath resources_pack_path;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  PathService::Get(base::DIR_MODULE, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
#else
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
#endif
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);
}
