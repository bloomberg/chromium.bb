// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"

using extensions::RulesRegistry;
using extensions::RulesRegistryService;
using extensions::WebRequestRulesRegistry;

namespace {
const char kTestEvent[] = "experimental.webRequest.onRequest";
}  // namespace

class DeclarativeApiTest : public ExtensionApiTest {
 public:
  DeclarativeApiTest() : rules_registry_(NULL) {}
  virtual ~DeclarativeApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  void RegisterTestRuleRegistry() {
    Profile* profile = browser()->profile();
    RulesRegistryService* rules_registry_service =
        profile->GetExtensionService()->GetRulesRegistryService();
    rules_registry_ = new WebRequestRulesRegistry();
    // TODO(battre): The registry should register itself when the
    // RulesRegistryService is instantiated.
    rules_registry_service->RegisterRulesRegistry(kTestEvent, rules_registry_);
  }

  scoped_refptr<RulesRegistry> rules_registry_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, DeclarativeApi) {
  RegisterTestRuleRegistry();
  ASSERT_TRUE(RunExtensionTest("declarative/api")) << message_;

  // Check that unloading the page has removed all rules.
  std::string extension_id = GetSingleLoadedExtension()->id();
  UnloadExtension(extension_id);

  // UnloadExtension posts a task to the owner thread of the extension
  // to process this unloading. The next task to retrive all rules
  // is therefore processed after the UnloadExtension task has been executed.

  std::vector<linked_ptr<RulesRegistry::Rule> > known_rules;

  content::BrowserThread::PostTask(
      rules_registry_->GetOwnerThread(),
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RulesRegistry::GetAllRules),
                 rules_registry_, extension_id, &known_rules));

  ui_test_utils::RunAllPendingInMessageLoop(rules_registry_->GetOwnerThread());

  EXPECT_TRUE(known_rules.empty());
}
