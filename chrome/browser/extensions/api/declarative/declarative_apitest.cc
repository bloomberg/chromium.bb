// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"

using extensions::RulesRegistry;
using extensions::RulesRegistryService;
using extensions::TestRulesRegistry;

namespace {
const char kTestEvent[] = "webRequest.onRequest";
}  // namespace

class DeclarativeApiTest : public ExtensionApiTest {
 public:
  DeclarativeApiTest() : test_rules_registry_(NULL) {}
  virtual ~DeclarativeApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  void RegisterTestRuleRegistry() {
    Profile* profile = browser()->profile();
    RulesRegistryService* rules_registry_service =
        profile->GetExtensionService()->GetRulesRegistryService();
    test_rules_registry_ = new TestRulesRegistry();
    // TODO(battre): Remove this, once we have a real implementation for a
    // RulesRegistry.
    rules_registry_service->RegisterRulesRegistry(kTestEvent,
                                                  test_rules_registry_);
  }

  scoped_refptr<RulesRegistry> test_rules_registry_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, DeclarativeApi) {
  RegisterTestRuleRegistry();
  ASSERT_TRUE(RunExtensionTest("declarative/api")) << message_;

  // Check that unloading the page has removed all rules.
  std::string extension_id = GetSingleLoadedExtension()->id();
  UnloadExtension(extension_id);

  // Make sure that RulesRegistry had a chance to process unloading the
  // extension.
  ui_test_utils::RunAllPendingInMessageLoop(
      test_rules_registry_->GetOwnerThread());

  std::vector<linked_ptr<RulesRegistry::Rule> > known_rules;

  test_rules_registry_->GetAllRules(extension_id, &known_rules);
  EXPECT_TRUE(known_rules.empty());
}
