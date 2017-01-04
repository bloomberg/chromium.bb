// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

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
  ~PreferencesBrowserTest() override;

  // InProcessBrowserTest implementation:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void OnPreferenceChanged(const std::string& pref_name);

 protected:
  MOCK_METHOD1(OnCommit, void(const PrefService::Preference*));

  // The pref service that holds the current pref values in the C++ backend.
  PrefService* pref_service();

  void SetUpPrefs();

  // InProcessBrowserTest implementation:
  void SetUpInProcessBrowserTestFixture() override;

  // Sets user policies through the mock policy provider.
  void SetUserPolicies(const std::vector<std::string>& names,
                       const std::vector<std::unique_ptr<base::Value>>& values,
                       policy::PolicyLevel level);
  // Clears user policies.
  void ClearUserPolicies();
  // Set user-modified pref values directly in the C++ backend.
  void SetUserValues(const std::vector<std::string>& names,
                     const std::vector<std::unique_ptr<base::Value>>& values);

  // Verifies that a dictionary contains a (key, value) pair. Takes ownership of
  // |expected|.
  void VerifyKeyValue(const base::DictionaryValue& dict,
                      const std::string& key,
                      const base::Value& expected);
  // Verifies that a dictionary contains a given pref and that its value has
  // been decorated correctly.
  void VerifyPref(const base::DictionaryValue* prefs,
                  const std::string& name,
                  const std::unique_ptr<base::Value>& value,
                  const std::string& controlledBy,
                  bool disabled,
                  bool uncommitted);
  // Verifies that a notification received from the JavaScript Preferences
  // class contains a given pref and that its value has been decorated
  // correctly.
  void VerifyObservedPref(const std::string& observed_json,
                          const std::string& name,
                          const std::unique_ptr<base::Value>& value,
                          const std::string& controlledBy,
                          bool disabled,
                          bool uncommitted);
  // Verifies that notifications received from the JavaScript Preferences class
  // contain the given prefs and that their values have been decorated
  // correctly.
  void VerifyObservedPrefs(
      const std::string& observed_json,
      const std::vector<std::string>& names,
      const std::vector<std::unique_ptr<base::Value>>& values,
      const std::string& controlledBy,
      bool disabled,
      bool uncommitted);

  // Sets up the expectation that the JavaScript Preferences class will make no
  // change to a user-modified pref value in the C++ backend.
  void ExpectNoCommit(const std::string& name);
  // Sets up the expectation that the JavaScript Preferences class will set a
  // user-modified pref value in the C++ backend.
  void ExpectSetCommit(const std::string& name,
                       const std::unique_ptr<base::Value>& value);
  // Sets up the expectation that the JavaScript Preferences class will clear a
  // user-modified pref value in the C++ backend.
  void ExpectClearCommit(const std::string& name);
  // Verifies that previously set expectations are met and clears them.
  void VerifyAndClearExpectations();

  // Sets up the JavaScript part of the test environment.
  void SetupJavaScriptTestEnvironment(
      const std::vector<std::string>& pref_names,
      std::string* observed_json) const;

  // Sets a value through the JavaScript Preferences class as if the user had
  // modified it. Returns the observation which can be verified using the
  // VerifyObserved* methods.
  void SetPref(const std::string& name,
               const std::string& type,
               const std::unique_ptr<base::Value>& value,
               bool commit,
               std::string* observed_json);

  // Verifies that setting a user-modified pref value through the JavaScript
  // Preferences class fires the correct notification in JavaScript and commits
  // the change to C++ if |commit| is true.
  void VerifySetPref(const std::string& name,
                     const std::string& type,
                     const std::unique_ptr<base::Value>& value,
                     bool commit);
  // Verifies that clearing a user-modified pref value through the JavaScript
  // Preferences class fires the correct notification in JavaScript and does
  // respectively does not cause the change to be committed to the C++ backend.
  void VerifyClearPref(const std::string& name,
                       const std::unique_ptr<base::Value>& value,
                       bool commit);
  // Verifies that committing a previously made change of a user-modified pref
  // value through the JavaScript Preferences class fires the correct
  // notification in JavaScript.
  void VerifyCommit(const std::string& name,
                    const std::unique_ptr<base::Value>& value,
                    const std::string& controlledBy);
  // Verifies that committing a previously set user-modified pref value through
  // the JavaScript Preferences class fires the correct notification in
  // JavaScript and causes the change to be committed to the C++ backend.
  void VerifySetCommit(const std::string& name,
                       const std::unique_ptr<base::Value>& value);
  // Verifies that committing the previously cleared user-modified pref value
  // through the JavaScript Preferences class fires the correct notification in
  // JavaScript and causes the change to be committed to the C++ backend.
  void VerifyClearCommit(const std::string& name,
                         const std::unique_ptr<base::Value>& value);
  // Verifies that rolling back a previously made change of a user-modified pref
  // value through the JavaScript Preferences class fires the correct
  // notification in JavaScript and does not cause the change to be committed to
  // the C++ backend.
  void VerifyRollback(const std::string& name,
                      const std::unique_ptr<base::Value>& value,
                      const std::string& controlledBy);
  // Start observing notifications sent by the JavaScript Preferences class for
  // pref values changes.
  void StartObserving();
  // Change the value of a sentinel pref in the C++ backend and finish observing
  // notifications sent by the JavaScript Preferences class when the
  // notification for this pref is received.
  void FinishObserving(std::string* observed_json);

  // Populate the lists of test prefs and corresponding policies with default
  // values used by most tests.
  void UseDefaultTestPrefs(bool includeListPref);

  // The current tab's render view host, required to inject JavaScript code into
  // the tab.
  content::RenderViewHost* render_view_host_;

  // Mock policy provider for both user and device policies.
  policy::MockConfigurationPolicyProvider policy_provider_;

  // Pref change registrar that detects changes to user-modified pref values
  // made in the C++ backend by the JavaScript Preferences class.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // The prefs and corresponding policies used by the current test.
  std::vector<std::string> types_;
  std::vector<std::string> pref_names_;
  std::vector<std::string> policy_names_;
  std::vector<std::unique_ptr<base::Value>> default_values_;
  std::vector<std::unique_ptr<base::Value>> non_default_values_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_
