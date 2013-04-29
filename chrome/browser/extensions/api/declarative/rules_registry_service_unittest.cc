// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"
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
  std::string error = registry->GetAllRules(kExtensionId, &get_rules);
  EXPECT_EQ(expected_number_of_rules, get_rules.size());
}

}  // namespace

namespace extensions {

class RulesRegistryServiceTest : public testing::Test {
 public:
  RulesRegistryServiceTest()
      : ui(content::BrowserThread::UI, &message_loop),
        io(content::BrowserThread::IO, &message_loop) {}

  virtual ~RulesRegistryServiceTest() {}

  virtual void TearDown() OVERRIDE {
    // Make sure that deletion traits of all registries are executed.
    message_loop.RunUntilIdle();
  }

 protected:
  MessageLoop message_loop;
  content::TestBrowserThread ui;
  content::TestBrowserThread io;
};

TEST_F(RulesRegistryServiceTest, TestConstructionAndMultiThreading) {
  TestRulesRegistry* ui_registry =
      new TestRulesRegistry(content::BrowserThread::UI, "ui");

  TestRulesRegistry* io_registry =
      new TestRulesRegistry(content::BrowserThread::IO, "io");

  // Test registration.

  RulesRegistryService registry_service(NULL);
  registry_service.RegisterRulesRegistry(make_scoped_refptr(ui_registry));
  registry_service.RegisterRulesRegistry(make_scoped_refptr(io_registry));

  EXPECT_TRUE(registry_service.GetRulesRegistry("ui").get());
  EXPECT_TRUE(registry_service.GetRulesRegistry("io").get());
  EXPECT_FALSE(registry_service.GetRulesRegistry("foo").get());

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&InsertRule, registry_service.GetRulesRegistry("ui"),
                 "ui_task"));

  content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InsertRule, registry_service.GetRulesRegistry("io"),
                   "io_task"));

  content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry("ui"), 1));

  content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry("io"), 1));

  message_loop.RunUntilIdle();

  // Test extension unloading.

  registry_service.SimulateExtensionUnloaded(kExtensionId);

  content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry("ui"), 0));

  content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&VerifyNumberOfRules,
                   registry_service.GetRulesRegistry("io"), 0));

  message_loop.RunUntilIdle();
}

}  // namespace extensions
