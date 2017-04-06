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

// This class does work in the constructor instead of SetUp() to give subclasses
// a valid BrowserContext to use while initializing their members. For example:
//
// class MyExtensionsTest : public ExtensionsTest {
//   MyExtensionsTest()
//     : my_object_(browser_context())) {
//   }
// };
// TODO(crbug.com/708256): All these instances are setup in the constructor, but
// destroyed in TearDown(), which may cause problems. Move this initialization
// to SetUp().
ExtensionsTest::ExtensionsTest()
    : content_browser_client_(new TestContentBrowserClient),
      content_utility_client_(new TestContentUtilityClient),
      browser_context_(new content::TestBrowserContext),
      incognito_context_(CreateTestIncognitoContext()),
      extensions_browser_client_(
          new TestExtensionsBrowserClient(browser_context_.get())) {
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
}

ExtensionsTest::~ExtensionsTest() {
  ExtensionsBrowserClient::Set(nullptr);
  content::SetBrowserClientForTesting(nullptr);
  content::SetUtilityClientForTesting(nullptr);
}

void ExtensionsTest::SetUp() {
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

  // TODO(crbug.com/708256): |extension_browser_client_| is reset here but not
  // unset as the singleton until the destructor. This can lead to use after
  // free errors.
  extensions_browser_client_.reset();
  browser_context_.reset();
  incognito_context_.reset();
  pref_service_.reset();
}

}  // namespace extensions
