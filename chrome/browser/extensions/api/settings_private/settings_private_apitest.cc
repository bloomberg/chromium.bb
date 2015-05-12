// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

const char kTestPrefName[] = "download.default_directory";
const char kTestPrefValue[] = "/Downloads";

class TestDelegate : public SettingsPrivateDelegate {
 public:
  explicit TestDelegate(Profile* profile) : SettingsPrivateDelegate(profile) {}

  bool SetPref(const std::string& name, const base::Value* value) override {
    // Write to the actual pref service, so that the SettingsPrivateEventRouter
    // dispatches an onPrefsChanged event.
    PrefService* pref_service = profile_->GetPrefs();
    pref_service->Set(name.c_str(), *value);
    return true;
  }

  scoped_ptr<base::Value> GetPref(const std::string& name) override {
    if (name.compare(kTestPrefName) != 0)
      return base::Value::CreateNullValue();

    return CreateTestPrefObject()->ToValue();
  }

  scoped_ptr<base::Value> GetAllPrefs() override {
    base::ListValue* list_value = new base::ListValue();
    list_value->Append(CreateTestPrefObject()->ToValue().release());
    return make_scoped_ptr(list_value);
  }

  ~TestDelegate() override {}

 private:
  scoped_ptr<api::settings_private::PrefObject> CreateTestPrefObject() {
    scoped_ptr<api::settings_private::PrefObject> pref_object(
        new api::settings_private::PrefObject());
    pref_object->key = std::string(kTestPrefName);
    pref_object->type = api::settings_private::PrefType::PREF_TYPE_STRING;
    pref_object->value.reset(new base::StringValue(kTestPrefValue));
    return pref_object.Pass();
  }

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SettingsPrivateApiTest : public ExtensionApiTest {
 public:
  SettingsPrivateApiTest() {}
  ~SettingsPrivateApiTest() override {}

  static KeyedService* GetSettingsPrivateDelegate(
      content::BrowserContext* profile) {
    CHECK(s_test_delegate_);
    return s_test_delegate_;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    if (!s_test_delegate_)
      s_test_delegate_ = new TestDelegate(profile());

    SettingsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), &SettingsPrivateApiTest::GetSettingsPrivateDelegate);
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunSettingsSubtest(const std::string& subtest) {
    return RunExtensionSubtest("settings_private",
                               "main.html?" + subtest,
                               kFlagLoadAsComponent);
  }

  // Static pointer to the TestDelegate so that it can be accessed in
  // GetSettingsPrivateDelegate() passed to SetTestingFactory().
  static TestDelegate* s_test_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateApiTest);
};

// static
TestDelegate* SettingsPrivateApiTest::s_test_delegate_ = NULL;

}  // namespace

IN_PROC_BROWSER_TEST_F(SettingsPrivateApiTest, SetPref) {
  EXPECT_TRUE(RunSettingsSubtest("setPref")) << message_;
}

IN_PROC_BROWSER_TEST_F(SettingsPrivateApiTest, GetPref) {
  EXPECT_TRUE(RunSettingsSubtest("getPref")) << message_;
}

IN_PROC_BROWSER_TEST_F(SettingsPrivateApiTest, GetAllPrefs) {
  EXPECT_TRUE(RunSettingsSubtest("getAllPrefs")) << message_;
}

IN_PROC_BROWSER_TEST_F(SettingsPrivateApiTest, OnPrefsChanged) {
  EXPECT_TRUE(RunSettingsSubtest("onPrefsChanged")) << message_;
}

}  // namespace extensions
