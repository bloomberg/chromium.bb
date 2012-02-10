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

using namespace extensions;

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
    RulesRegistryService* rules_registry =
        profile->GetExtensionService()->GetRulesRegistryService();
    scoped_ptr<RulesRegistry> test_rules_registry(new TestRulesRegistry());
    test_rules_registry_ = test_rules_registry.get();
    // TODO(battre): Remove this, once we have a real implementation for a
    // RulesRegistry.
    rules_registry->RegisterRulesRegistry(kTestEvent,
                                          test_rules_registry.Pass());
  }

  // Weak pointer that is deleted when the profile is destroyed.
  RulesRegistry* test_rules_registry_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, DeclarativeApi) {
  RegisterTestRuleRegistry();
  ASSERT_TRUE(RunExtensionTest("declarative/api")) << message_;

  // Check that unloading the page has removed all rules.
  std::string extension_id = GetSingleLoadedExtension()->id();
  UnloadExtension(extension_id);

  std::vector<std::string> rule_identifiers;  // Empty to get all rules.
  std::vector<DictionaryValue*> known_rules;
  test_rules_registry_->GetRules(extension_id,
                                 rule_identifiers,
                                 &known_rules);
  EXPECT_TRUE(known_rules.empty());
}
