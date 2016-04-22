// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_unit_test_suite.h"

#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/translate/content/browser/browser_cld_data_provider_factory.h"
#include "components/translate/content/common/cld_data_source.h"
#include "components/update_client/update_query_params.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_paths.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "extensions/common/extension_paths.h"
#endif

namespace {

// Creates a TestingBrowserProcess for each test.
class ChromeUnitTestSuiteInitializer : public testing::EmptyTestEventListener {
 public:
  ChromeUnitTestSuiteInitializer() {}
  virtual ~ChromeUnitTestSuiteInitializer() {}

  void OnTestStart(const testing::TestInfo& test_info) override {
    content_client_.reset(new ChromeContentClient);
    content::SetContentClient(content_client_.get());
    browser_content_client_.reset(new ChromeContentBrowserClient());
    content::SetBrowserClientForTesting(browser_content_client_.get());
    utility_content_client_.reset(new ChromeContentUtilityClient());
    content::SetUtilityClientForTesting(utility_content_client_.get());

    TestingBrowserProcess::CreateInstance();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    browser_content_client_.reset();
    utility_content_client_.reset();
    content_client_.reset();
    content::SetContentClient(NULL);

    // AsyncPolicyProvider is a lazily created KeyedService that may need to be
    // shut down here. However, AsyncPolicyProvider::Shutdown() will want to
    // post tasks to delete its policy loaders. This goes through
    // BrowserThreadTaskRunner::PostNonNestableDelayedTask(), which can invoke
    // LazyInstance<BrowserThreadGlobals>::Get() and try to create it for the
    // first time. It might be created during the test, but it might not (see
    // comments in TestingBrowserProcess::browser_policy_connector()). Since
    // creating BrowserThreadGlobals requires creating a SequencedWorkerPool,
    // and that needs a MessageLoop, make sure there is one here so that tests
    // don't get obscure errors. Tests can also invoke TestingBrowserProcess::
    // DeleteInstance() themselves (after ensuring any TestingProfile instances
    // are deleted). But they shouldn't have to worry about that.
    DCHECK(!base::MessageLoop::current());
    base::MessageLoopForUI message_loop;
    TestingBrowserProcess::DeleteInstance();
  }

 private:
  // Client implementations for the content module.
  std::unique_ptr<ChromeContentClient> content_client_;
  std::unique_ptr<ChromeContentBrowserClient> browser_content_client_;
  std::unique_ptr<ChromeContentUtilityClient> utility_content_client_;

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

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
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
  translate::BrowserCldDataProviderFactory::SetDefault(
      new translate::BrowserCldDataProviderFactory());
  translate::CldDataSource::SetDefault(
      translate::CldDataSource::GetStaticDataSource());
  component_updater::RegisterPathProvider(chrome::DIR_USER_DATA);

#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif

#if defined(ENABLE_EXTENSIONS)
  extensions::RegisterPathProvider();

  extensions::ExtensionsClient::Set(
      extensions::ChromeExtensionsClient::GetInstance());
#endif

  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());

  gfx::GLSurfaceTestSupport::InitializeOneOff();

  update_client::UpdateQueryParams::SetDelegate(
      ChromeUpdateQueryParamsDelegate::GetInstance());
}

void ChromeUnitTestSuite::InitializeResourceBundle() {
  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);
}
