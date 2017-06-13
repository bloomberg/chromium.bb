// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_test.h"

#include "base/memory/ptr_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/test/test_content_browser_client.h"
#include "extensions/test/test_content_utility_client.h"

namespace {

std::unique_ptr<content::TestBrowserContext> CreateTestIncognitoContext() {
  std::unique_ptr<content::TestBrowserContext> incognito_context =
      base::MakeUnique<content::TestBrowserContext>();
  incognito_context->set_is_off_the_record(true);
  return incognito_context;
}

}  // namespace

namespace extensions {

ExtensionsTest::ExtensionsTest()
    : rvh_test_enabler_(
          base::MakeUnique<content::RenderViewHostTestEnabler>()) {}

ExtensionsTest::ExtensionsTest(
    std::unique_ptr<content::TestBrowserThreadBundle> thread_bundle)
    : thread_bundle_(std::move(thread_bundle)),
      rvh_test_enabler_(
          base::MakeUnique<content::RenderViewHostTestEnabler>()) {}

ExtensionsTest::~ExtensionsTest() {
  // Destroy the task runners before nulling the browser/utility clients, as
  // posted tasks may use them.
  rvh_test_enabler_.reset();
  thread_bundle_.reset();
  content::SetBrowserClientForTesting(nullptr);
  content::SetUtilityClientForTesting(nullptr);
}

void ExtensionsTest::SetUp() {
  content_browser_client_ = base::MakeUnique<TestContentBrowserClient>();
  content_utility_client_ = base::MakeUnique<TestContentUtilityClient>();
  browser_context_ = base::MakeUnique<content::TestBrowserContext>();
  incognito_context_ = CreateTestIncognitoContext();
  extensions_browser_client_ =
      base::MakeUnique<TestExtensionsBrowserClient>(browser_context_.get());

  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
      browser_context_.get());
  content::SetBrowserClientForTesting(content_browser_client_.get());
  content::SetUtilityClientForTesting(content_utility_client_.get());
  ExtensionsBrowserClient::Set(extensions_browser_client_.get());
  extensions_browser_client_->set_extension_system_factory(
      &extension_system_factory_);
  extensions_browser_client_->SetIncognitoContext(incognito_context_.get());

  // Set up all the dependencies of ExtensionPrefs.
  extension_pref_value_map_.reset(new ExtensionPrefValueMap());
  PrefServiceFactory factory;
  factory.set_user_prefs(new TestingPrefStore());
  factory.set_extension_prefs(new TestingPrefStore());
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable();
  // Prefs should be registered before the PrefService is created.
  ExtensionPrefs::RegisterProfilePrefs(pref_registry);
  pref_service_ = factory.Create(pref_registry);

  std::unique_ptr<ExtensionPrefs> extension_prefs(ExtensionPrefs::Create(
      browser_context(), pref_service_.get(),
      browser_context()->GetPath().AppendASCII("Extensions"),
      extension_pref_value_map_.get(), false /* extensions_disabled */,
      std::vector<ExtensionPrefsObserver*>()));

  ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
      browser_context(), std::move(extension_prefs));

  // Crashing here? Don't use this class in Chrome's unit_tests. See header.
  BrowserContextDependencyManager::GetInstance()
      ->CreateBrowserContextServicesForTest(browser_context_.get());
}

void ExtensionsTest::TearDown() {
  // Allows individual tests to have BrowserContextKeyedServiceFactory objects
  // as member variables instead of singletons. The individual services will be
  // cleaned up before the factories are destroyed.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());

  extensions_browser_client_.reset();
  ExtensionsBrowserClient::Set(nullptr);

  browser_context_.reset();
  incognito_context_.reset();
  pref_service_.reset();
}

}  // namespace extensions
