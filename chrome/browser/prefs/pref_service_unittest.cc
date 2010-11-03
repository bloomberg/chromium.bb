// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/test/data/resource.h"
#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/dummy_pref_store.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::Pointee;
using testing::Property;

namespace {

class TestPrefObserver : public NotificationObserver {
 public:
  TestPrefObserver(const PrefService* prefs,
                   const std::string& pref_name,
                   const std::string& new_pref_value)
      : observer_fired_(false),
        prefs_(prefs),
        pref_name_(pref_name),
        new_pref_value_(new_pref_value) {}
  virtual ~TestPrefObserver() {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    EXPECT_EQ(type.value, NotificationType::PREF_CHANGED);
    const PrefService* prefs_in = Source<PrefService>(source).ptr();
    EXPECT_EQ(prefs_in, prefs_);
    const std::string* pref_name_in = Details<std::string>(details).ptr();
    EXPECT_EQ(*pref_name_in, pref_name_);
    EXPECT_EQ(new_pref_value_, prefs_in->GetString("homepage"));
    observer_fired_ = true;
  }

  bool observer_fired() { return observer_fired_; }

  void Reset(const std::string& new_pref_value) {
    observer_fired_ = false;
    new_pref_value_ = new_pref_value;
  }

 private:
  bool observer_fired_;
  const PrefService* prefs_;
  const std::string pref_name_;
  std::string new_pref_value_;
};

}  // namespace

// TODO(port): port this test to POSIX.
#if defined(OS_WIN)
TEST(PrefServiceTest, LocalizedPrefs) {
  TestingPrefService prefs;
  const char kBoolean[] = "boolean";
  const char kInteger[] = "integer";
  const char kString[] = "string";
  prefs.RegisterLocalizedBooleanPref(kBoolean, IDS_LOCALE_BOOL);
  prefs.RegisterLocalizedIntegerPref(kInteger, IDS_LOCALE_INT);
  prefs.RegisterLocalizedStringPref(kString, IDS_LOCALE_STRING);

  // The locale default should take preference over the user default.
  EXPECT_FALSE(prefs.GetBoolean(kBoolean));
  EXPECT_EQ(1, prefs.GetInteger(kInteger));
  EXPECT_EQ("hello", prefs.GetString(kString));

  prefs.SetBoolean(kBoolean, true);
  EXPECT_TRUE(prefs.GetBoolean(kBoolean));
  prefs.SetInteger(kInteger, 5);
  EXPECT_EQ(5, prefs.GetInteger(kInteger));
  prefs.SetString(kString, "foo");
  EXPECT_EQ("foo", prefs.GetString(kString));
}
#endif

TEST(PrefServiceTest, NoObserverFire) {
  TestingPrefService prefs;

  const char pref_name[] = "homepage";
  prefs.RegisterStringPref(pref_name, std::string());

  const std::string new_pref_value("http://www.google.com/");
  TestPrefObserver obs(&prefs, pref_name, new_pref_value);

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(pref_name, &obs);
  // This should fire the checks in TestPrefObserver::Observe.
  prefs.SetString(pref_name, new_pref_value);

  // Make sure the observer was actually fired.
  EXPECT_TRUE(obs.observer_fired());

  // Setting the pref to the same value should not set the pref value a second
  // time.
  obs.Reset(new_pref_value);
  prefs.SetString(pref_name, new_pref_value);
  EXPECT_FALSE(obs.observer_fired());

  // Clearing the pref should cause the pref to fire.
  obs.Reset(std::string());
  prefs.ClearPref(pref_name);
  EXPECT_TRUE(obs.observer_fired());

  // Clearing the pref again should not cause the pref to fire.
  obs.Reset(std::string());
  prefs.ClearPref(pref_name);
  EXPECT_FALSE(obs.observer_fired());
}

TEST(PrefServiceTest, HasPrefPath) {
  TestingPrefService prefs;

  const char path[] = "fake.path";

  // Shouldn't initially have a path.
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Register the path. This doesn't set a value, so the path still shouldn't
  // exist.
  prefs.RegisterStringPref(path, std::string());
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Set a value and make sure we have a path.
  prefs.SetString(path, "blah");
  EXPECT_TRUE(prefs.HasPrefPath(path));
}

TEST(PrefServiceTest, Observers) {
  const char pref_name[] = "homepage";

  TestingPrefService prefs;
  prefs.SetUserPref(pref_name, Value::CreateStringValue("http://www.cnn.com"));
  prefs.RegisterStringPref(pref_name, std::string());

  const std::string new_pref_value("http://www.google.com/");
  TestPrefObserver obs(&prefs, pref_name, new_pref_value);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(pref_name, &obs);
  // This should fire the checks in TestPrefObserver::Observe.
  prefs.SetString(pref_name, new_pref_value);

  // Make sure the tests were actually run.
  EXPECT_TRUE(obs.observer_fired());

  // Now try adding a second pref observer.
  const std::string new_pref_value2("http://www.youtube.com/");
  obs.Reset(new_pref_value2);
  TestPrefObserver obs2(&prefs, pref_name, new_pref_value2);
  registrar.Add(pref_name, &obs2);
  // This should fire the checks in obs and obs2.
  prefs.SetString(pref_name, new_pref_value2);
  EXPECT_TRUE(obs.observer_fired());
  EXPECT_TRUE(obs2.observer_fired());

  // Make sure obs2 still works after removing obs.
  registrar.Remove(pref_name, &obs);
  obs.Reset(std::string());
  obs2.Reset(new_pref_value);
  // This should only fire the observer in obs2.
  prefs.SetString(pref_name, new_pref_value);
  EXPECT_FALSE(obs.observer_fired());
  EXPECT_TRUE(obs2.observer_fired());
}

TEST(PrefServiceTest, ProxyFromCommandLineNotPolicy) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  TestingPrefService prefs(NULL, &command_line);
  browser::RegisterUserPrefs(&prefs);
  EXPECT_TRUE(prefs.GetBoolean(prefs::kProxyAutoDetect));
  const PrefService::Preference* pref =
      prefs.FindPreference(prefs::kProxyAutoDetect);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsManaged());
}

TEST(PrefServiceTest, ProxyPolicyOverridesCommandLineOptions) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line.AppendSwitchASCII(switches::kProxyPacUrl, "456");
  command_line.AppendSwitchASCII(switches::kProxyServer, "789");
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_value = Value::CreateIntegerValue(
      policy::kPolicyManuallyConfiguredProxyMode);
  provider->AddPolicy(policy::kPolicyProxyServerMode, mode_value);
  provider->AddPolicy(policy::kPolicyProxyBypassList,
                      Value::CreateStringValue("abc"));
  provider->AddPolicy(policy::kPolicyProxyPacUrl,
                      Value::CreateStringValue("def"));
  provider->AddPolicy(policy::kPolicyProxyServer,
                      Value::CreateStringValue("ghi"));

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  TestingPrefService prefs(NULL, &command_line);
  browser::RegisterUserPrefs(&prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_FALSE(prefs.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ("789", prefs.GetString(prefs::kProxyServer));
  EXPECT_EQ("456", prefs.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ("123", prefs.GetString(prefs::kProxyBypassList));

  // Try a second time time with the managed PrefStore in place, the
  // manual proxy policy should have removed all traces of the command
  // line and replaced them with the policy versions.
  TestingPrefService prefs2(provider.get(), &command_line);
  browser::RegisterUserPrefs(&prefs2);
  EXPECT_FALSE(prefs2.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_FALSE(prefs2.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ("ghi", prefs2.GetString(prefs::kProxyServer));
  EXPECT_EQ("def", prefs2.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ("abc", prefs2.GetString(prefs::kProxyBypassList));
}

TEST(PrefServiceTest, ProxyPolicyOverridesUnrelatedCommandLineOptions) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line.AppendSwitchASCII(switches::kProxyPacUrl, "456");
  command_line.AppendSwitchASCII(switches::kProxyServer, "789");
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_value = Value::CreateIntegerValue(
      policy::kPolicyUseSystemProxyMode);
  provider->AddPolicy(policy::kPolicyProxyServerMode, mode_value);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  TestingPrefService prefs(NULL, &command_line);
  browser::RegisterUserPrefs(&prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_FALSE(prefs.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ("789", prefs.GetString(prefs::kProxyServer));
  EXPECT_EQ("456", prefs.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ("123", prefs.GetString(prefs::kProxyBypassList));

  // Try a second time time with the managed PrefStore in place, the
  // no proxy policy should have removed all traces of the command
  // line proxy settings, even though they were not the specific one
  // set in policy.
  TestingPrefService prefs2(provider.get(), &command_line);
  browser::RegisterUserPrefs(&prefs2);
  EXPECT_FALSE(prefs2.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_FALSE(prefs2.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyServer));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyBypassList));
}

TEST(PrefServiceTest, ProxyPolicyOverridesCommandLineNoProxy) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kNoProxyServer);
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_value = Value::CreateIntegerValue(
      policy::kPolicyAutoDetectProxyMode);
  provider->AddPolicy(policy::kPolicyProxyServerMode, mode_value);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  TestingPrefService prefs(NULL, &command_line);
  browser::RegisterUserPrefs(&prefs);
  EXPECT_FALSE(prefs.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_TRUE(prefs.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ(std::string(), prefs.GetString(prefs::kProxyServer));
  EXPECT_EQ(std::string(), prefs.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ(std::string(), prefs.GetString(prefs::kProxyBypassList));

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  TestingPrefService prefs2(provider.get(), &command_line);
  browser::RegisterUserPrefs(&prefs2);
  EXPECT_TRUE(prefs2.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_FALSE(prefs2.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyServer));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyBypassList));
}

TEST(PrefServiceTest, ProxyPolicyOverridesCommandLineAutoDetect) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_value = Value::CreateIntegerValue(
      policy::kPolicyNoProxyServerMode);
  provider->AddPolicy(policy::kPolicyProxyServerMode, mode_value);

  // First verify that the auto-detect is set if there is no managed
  // PrefStore.
  TestingPrefService prefs(NULL, &command_line);
  browser::RegisterUserPrefs(&prefs);
  EXPECT_TRUE(prefs.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_FALSE(prefs.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ(std::string(), prefs.GetString(prefs::kProxyServer));
  EXPECT_EQ(std::string(), prefs.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ(std::string(), prefs.GetString(prefs::kProxyBypassList));

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  TestingPrefService prefs2(provider.get(), &command_line);
  browser::RegisterUserPrefs(&prefs2);
  EXPECT_FALSE(prefs2.GetBoolean(prefs::kProxyAutoDetect));
  EXPECT_TRUE(prefs2.GetBoolean(prefs::kNoProxyServer));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyServer));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyPacUrl));
  EXPECT_EQ(std::string(), prefs2.GetString(prefs::kProxyBypassList));
}

class PrefServiceSetValueTest : public testing::Test {
 protected:
  static const char kName[];
  static const char kValue[];

  PrefServiceSetValueTest()
      : name_string_(kName),
        null_value_(Value::CreateNullValue()) {}

  void SetExpectNoNotification() {
    EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  }

  void SetExpectPrefChanged() {
    EXPECT_CALL(observer_,
                Observe(NotificationType(NotificationType::PREF_CHANGED), _,
                        Property(&Details<std::string>::ptr,
                                 Pointee(name_string_))));
  }

  TestingPrefService prefs_;
  std::string name_string_;
  scoped_ptr<Value> null_value_;
  NotificationObserverMock observer_;
};

const char PrefServiceSetValueTest::kName[] = "name";
const char PrefServiceSetValueTest::kValue[] = "value";

TEST_F(PrefServiceSetValueTest, SetStringValue) {
  const char default_string[] = "default";
  const scoped_ptr<Value> default_value(
      Value::CreateStringValue(default_string));
  prefs_.RegisterStringPref(kName, default_string);

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, &observer_);

  // Changing the controlling store from default to user triggers notification.
  SetExpectPrefChanged();
  prefs_.Set(kName, *default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  SetExpectNoNotification();
  prefs_.Set(kName, *default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  const scoped_ptr<Value> new_value(Value::CreateStringValue(kValue));
  SetExpectPrefChanged();
  prefs_.Set(kName, *new_value);
  EXPECT_EQ(kValue, prefs_.GetString(kName));
}

TEST_F(PrefServiceSetValueTest, SetDictionaryValue) {
  prefs_.RegisterDictionaryPref(kName);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, &observer_);

  // Dictionary values are special: setting one to NULL is the same as clearing
  // the user value, allowing the NULL default to take (or keep) control.
  SetExpectNoNotification();
  prefs_.Set(kName, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);

  DictionaryValue new_value;
  new_value.SetString(kName, kValue);
  SetExpectPrefChanged();
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
  DictionaryValue* dict = prefs_.GetMutableDictionary(kName);
  EXPECT_EQ(1U, dict->size());
  std::string out_value;
  dict->GetString(kName, &out_value);
  EXPECT_EQ(kValue, out_value);

  SetExpectNoNotification();
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  SetExpectPrefChanged();
  prefs_.Set(kName, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);
  dict = prefs_.GetMutableDictionary(kName);
  EXPECT_EQ(0U, dict->size());
}

TEST_F(PrefServiceSetValueTest, SetListValue) {
  prefs_.RegisterListPref(kName);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, &observer_);

  // List values are special: setting one to NULL is the same as clearing the
  // user value, allowing the NULL default to take (or keep) control.
  SetExpectNoNotification();
  prefs_.Set(kName, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);

  ListValue new_value;
  new_value.Append(Value::CreateStringValue(kValue));
  SetExpectPrefChanged();
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
  const ListValue* list = prefs_.GetMutableList(kName);
  ASSERT_EQ(1U, list->GetSize());
  std::string out_value;
  list->GetString(0, &out_value);
  EXPECT_EQ(kValue, out_value);

  SetExpectNoNotification();
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  SetExpectPrefChanged();
  prefs_.Set(kName, *null_value_);
  Mock::VerifyAndClearExpectations(&observer_);
  list = prefs_.GetMutableList(kName);
  EXPECT_EQ(0U, list->GetSize());
}
