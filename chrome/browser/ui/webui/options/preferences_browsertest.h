// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_constants.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace content {
class RenderViewHost;
}

// Tests verifying that the JavaScript Preferences class, the underlying C++
// CoreOptionsHandler and the specialized classes handling Chrome OS device and
// proxy prefs behave correctly.
class PreferencesBrowserTest : public InProcessBrowserTest {
 public:
  PreferencesBrowserTest();

  // InProcessBrowserTest implementation:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

 protected:
  // Verifies that a dictionary contains a (key, value) pair. Takes ownership of
  // |expected|.
  void VerifyValue(const base::DictionaryValue* dict,
                   const std::string& key,
                   base::Value* expected);
  // Verifies that a pref value has been decorated correctly.
  void VerifyDict(const base::DictionaryValue* dict,
                  const base::Value* value,
                  const std::string& controlledBy,
                  bool disabled);
  // Verifies that a dictionary contains a given pref and that its value has
  // been decorated correctly.
  void VerifyPref(const base::DictionaryValue* prefs,
                  const std::string& name,
                  const base::Value* value,
                  const std::string& controlledBy,
                  bool disabled);
  // Verifies that a dictionary contains a given list of prefs and that their
  // values have been decorated correctly.
  void VerifyPrefs(const base::DictionaryValue* prefs,
                   const std::vector<std::string>& names,
                   const std::vector<base::Value*>& values,
                   const std::string& controlledBy,
                   bool disabled);
  // Sets a pref value from JavaScript, waits for an observer callback to fire
  // and returns the decorated value received.
  void VerifySetPref(const std::string& name,
                     const std::string& type,
                     base::Value* set_value,
                     base::Value* expected_value);

  // Requests a list of pref values from JavaScript, waits for a callback to
  // fire and returns the decorated values received.
  void FetchPrefs(const std::vector<std::string>& names,
                  base::DictionaryValue** prefs);

  // Sets user policies through the mock policy provider.
  void SetUserPolicies(const std::vector<std::string>& names,
                       const std::vector<base::Value*>& values,
                       policy::PolicyLevel level);

  // Helper deleting a vector of values.
  void DeleteValues(std::vector<base::Value*>& values);

  // The current tab's render view host, required to inject JavaScript code into
  // the tab.
  content::RenderViewHost* render_view_host_;

  // Mock user policy provider.
  policy::MockConfigurationPolicyProvider policy_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_
