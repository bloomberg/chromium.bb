// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExtensionId[] = "foo";

void InsertRule(scoped_refptr<extensions::RulesRegistry> registry,
                const std::string& id) {
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > add_rules;
  add_rules.push_back(make_linked_ptr(new extensions::RulesRegistry::Rule));
  add_rules[0]->id.reset(new std::string(id));
  std::string error = registry->AddRules(kExtensionId, add_rules);
  EXPECT_TRUE(error.empty());
}

void VerifyNumberOfRules(scoped_refptr<extensions::RulesRegistry> registry,
                         size_t expected_number_of_rules) {
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > get_rules;
  registry->GetAllRules(kExtensionId, &get_rules);
  EXPECT_EQ(expected_number_of_rules, get_rules.size());
}

}  // namespace

namespace extensions {

class RulesRegistryServiceTest : public testing::Test {
 public:
  RulesRegistryServiceTest()
      : ui_(content::BrowserThread::UI, &message_loop_),
        io_(content::BrowserThread::IO, &message_loop_) {}

  virtual ~RulesRegistryServiceTest() {}

  virtual void TearDown() OVERRIDE {
    // Make sure that deletion traits of all registries are executed.
    message_loop_.RunUntilIdle();
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_;
  content::TestBrowserThread io_;
};

TEST_F(RulesRegistryServiceTest, TestConstructionAndMultiThreading) {
  const RulesRegistry::WebViewKey key(0, 0);
  TestRulesRegistry* ui_registry =
      new TestRulesRegistry(content::BrowserThread::UI, "ui", key);

  TestRulesRegistry* io_registry =
      new TestRulesRegistry(content::BrowserThread::IO, "io", key);

  // Test registration.

  RulesRegistryService registry_service(NULL);
  registry_service.RegisterRulesRegistry(make_scoped_refptr(ui_registry));
  registry_service.RegisterRulesRegistry(make_scoped_refptr(io_registry));

  EXPECT_TRUE(registry_service.GetRulesRegistry(key, "ui").get());
  EXPECT_TRUE(registry_service.GetRulesRegistry(key, "io").get());
  EXPECT_FALSE(registry_service.GetRulesRegistry(key, "foo").get());

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&InsertRule, registry_service.GetRulesRegistry(key, "ui"),
                 "ui_task"));

  content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InsertRule, registry_service.GetRulesRegistry(key, "io"),
                   "io_task"));

  content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry(key, "ui"), 1));

  content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry(key, "io"), 1));

  message_loop_.RunUntilIdle();

  // Test extension uninstalling.

  registry_service.SimulateExtensionUninstalled(kExtensionId);

  content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry(key, "ui"), 0));

  content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry(key, "io"), 0));

  message_loop_.RunUntilIdle();
}

// This test verifies that removing rules registries by process ID works as
// intended. This test ensures that removing registries associated with one
// Webview embedder process does not remove registries associated with the
// other.
TEST_F(RulesRegistryServiceTest, TestWebViewKey) {
  const int kEmbedderProcessID1 = 1;
  const int kEmbedderProcessID2 = 2;
  const int kWebViewInstanceID = 1;

  const RulesRegistry::WebViewKey key1(kEmbedderProcessID1, kWebViewInstanceID);
  const RulesRegistry::WebViewKey key2(kEmbedderProcessID2, kWebViewInstanceID);

  TestRulesRegistry* ui_registry_key1 =
      new TestRulesRegistry(content::BrowserThread::UI, "ui", key1);
  TestRulesRegistry* ui_registry_key2 =
      new TestRulesRegistry(content::BrowserThread::UI, "ui", key2);

  RulesRegistryService registry_service(NULL);
  registry_service.RegisterRulesRegistry(make_scoped_refptr(ui_registry_key1));
  registry_service.RegisterRulesRegistry(make_scoped_refptr(ui_registry_key2));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&InsertRule, registry_service.GetRulesRegistry(key1, "ui"),
                 "ui_task"));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&InsertRule, registry_service.GetRulesRegistry(key2, "ui"),
                 "ui_task"));
  message_loop_.RunUntilIdle();

  registry_service.RemoveWebViewRulesRegistries(kEmbedderProcessID1);
  EXPECT_FALSE(registry_service.GetRulesRegistry(key1, "ui").get());
  EXPECT_TRUE(registry_service.GetRulesRegistry(key2, "ui").get());
}

TEST_F(RulesRegistryServiceTest, TestWebViewWebRequestRegistryHasNoCache) {
  const int kEmbedderProcessID = 1;
  const int kWebViewInstanceID = 1;
  const RulesRegistry::WebViewKey key(kEmbedderProcessID, kWebViewInstanceID);
  TestingProfile profile;
  RulesRegistryService registry_service(&profile);
  RulesRegistry* registry =
      registry_service.GetRulesRegistry(
          key,
          declarative_webrequest_constants::kOnRequest).get();
  EXPECT_TRUE(registry);
  EXPECT_FALSE(registry->rules_cache_delegate_for_testing());
}

}  // namespace extensions
