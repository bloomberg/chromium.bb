// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace content {
class NotificationDetails;
class NotificationSource;
class RenderViewHost;
}

// Tests verifying that the JavaScript Preferences class, the underlying C++
// CoreOptionsHandler and the specialized classes handling Chrome OS device and
// proxy prefs behave correctly.
class PreferencesBrowserTest : public InProcessBrowserTest,
                               public content::NotificationObserver {
 public:
  PreferencesBrowserTest();
  ~PreferencesBrowserTest();

  // InProcessBrowserTest implementation:
  virtual void SetUpOnMainThread() OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  MOCK_METHOD1(OnCommit, void(const PrefService::Preference*));

  // InProcessBrowserTest implementation:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  // Sets user policies through the mock policy provider.
  void SetUserPolicies(const std::vector<std::string>& names,
                       const std::vector<base::Value*>& values,
                       policy::PolicyLevel level);
  // Clears user policies.
  void ClearUserPolicies();
  // Set user-modified pref values directly in the C++ backend.
  void SetUserValues(const std::vector<std::string>& names,
                     const std::vector<base::Value*>& values);
  // Helper deleting a vector of values.
  void DeleteValues(std::vector<base::Value*>& values);

  // Verifies that a dictionary contains a (key, value) pair. Takes ownership of
  // |expected|.
  void VerifyKeyValue(const base::DictionaryValue* dict,
                      const std::string& key,
                      base::Value* expected);
  // Verifies that a dictionary contains a given pref and that its value has
  // been decorated correctly.
  void VerifyPref(const base::DictionaryValue* prefs,
                  const std::string& name,
                  const base::Value* value,
                  const std::string& controlledBy,
                  bool disabled,
                  bool uncommitted);
  // Verifies that a notification received from the JavaScript Preferences
  // class contains a given pref and that its value has been decorated
  // correctly.
  void VerifyObservedPref(const std::string& observed_json,
                          const std::string& name,
                          const base::Value* value,
                          const std::string& controlledBy,
                          bool disabled,
                          bool uncommitted);
  // Verifies that notifications received from the JavaScript Preferences class
  // contain the given prefs and that their values have been decorated
  // correctly.
  void VerifyObservedPrefs(const std::string& observed_json,
                           const std::vector<std::string>& names,
                           const std::vector<base::Value*>& values,
                           const std::string& controlledBy,
                           bool disabled,
                           bool uncommitted);

  // Sets up the expectation that the JavaScript Preferences class will make no
  // change to a user-modified pref value in the C++ backend.
  void ExpectNoCommit(const std::string& name);
  // Sets up the expectation that the JavaScript Preferences class will set a
  // user-modified pref value in the C++ backend.
  void ExpectSetCommit(const std::string& name,
                       const base::Value* value);
  // Sets up the expectation that the JavaScript Preferences class will clear a
  // user-modified pref value in the C++ backend.
  void ExpectClearCommit(const std::string& name);
  // Verifies that previously set expectations are met and clears them.
  void VerifyAndClearExpectations();

  // Sets up the JavaScript part of the test environment.
  void SetupJavaScriptTestEnvironment(
      const std::vector<std::string>& pref_names,
      std::string* observed_json) const;
  // Verifies that setting a user-modified pref value through the JavaScript
  // Preferences class fires the correct notification in JavaScript and does
  // respectively does not cause the change to be committed to the C++ backend.
  void VerifySetPref(const std::string& name,
                     const std::string& type,
                     const base::Value* value,
                     bool commit);
  // Verifies that clearing a user-modified pref value through the JavaScript
  // Preferences class fires the correct notification in JavaScript and does
  // respectively does not cause the change to be committed to the C++ backend.
  void VerifyClearPref(const std::string& name,
                       const base::Value* value,
                       bool commit);
  // Verifies that committing a previously made change of a user-modified pref
  // value through the JavaScript Preferences class fires the correct
  // notification in JavaScript.
  void VerifyCommit(const std::string& name,
                    const base::Value* value,
                    const std::string& controlledBy);
  // Verifies that committing a previously set user-modified pref value through
  // the JavaScript Preferences class fires the correct notification in
  // JavaScript and causes the change to be committed to the C++ backend.
  void VerifySetCommit(const std::string& name,
                       const base::Value* value);
  // Verifies that committing the previously cleared user-modified pref value
  // through the JavaScript Preferences class fires the correct notification in
  // JavaScript and causes the change to be committed to the C++ backend.
  void VerifyClearCommit(const std::string& name,
                         const base::Value* value);
  // Verifies that rolling back a previously made change of a user-modified pref
  // value through the JavaScript Preferences class fires the correct
  // notification in JavaScript and does not cause the change to be committed to
  // the C++ backend.
  void VerifyRollback(const std::string& name,
                      const base::Value* value,
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

  // Mock user policy provider.
  policy::MockConfigurationPolicyProvider policy_provider_;

  // Pref change registrar that detects changes to user-modified pref values
  // made in the C++ backend by the JavaScript Preferences class.
  PrefChangeRegistrar pref_change_registrar_;

  // The pref service that holds the current pref values in the C++ backend.
  PrefService* pref_service_;

  // The prefs and corresponding policies used by the current test.
  std::vector<std::string> types_;
  std::vector<std::string> pref_names_;
  std::vector<std::string> policy_names_;
  std::vector<base::Value*> default_values_;
  std::vector<base::Value*> non_default_values_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PREFERENCES_BROWSERTEST_H_
