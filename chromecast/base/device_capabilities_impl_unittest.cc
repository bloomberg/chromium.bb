// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/device_capabilities_impl.h"

#include <string>

#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {

const char kSampleDictionaryCapability[] =
    "{"
    "  \"dummy_field_bool\": true,"
    "  \"dummy_field_int\": 99"
    "}";

// Simple capability manager that implements the Validator interface. Either
// accepts or rejects all proposed changes based on |accept_changes| constructor
// argument.
class FakeCapabilityManagerSimple : public DeviceCapabilities::Validator {
 public:
  FakeCapabilityManagerSimple(DeviceCapabilities* capabilities,
                              const std::string& key,
                              bool accept_changes)
      : DeviceCapabilities::Validator(capabilities),
        key_(key),
        accept_changes_(accept_changes) {}

  ~FakeCapabilityManagerSimple() override {}

  void Validate(const std::string& path,
                scoped_ptr<base::Value> proposed_value) override {
    ASSERT_TRUE(path.find(key_) == 0);
    if (accept_changes_)
      SetValidatedValue(path, proposed_value.Pass());
  }

 private:
  const std::string key_;
  const bool accept_changes_;
};

// Used to test that OnCapabilitiesChanged() is called when capabilities are
// modified
class MockCapabilitiesObserver : public DeviceCapabilities::Observer {
 public:
  MockCapabilitiesObserver() {}
  ~MockCapabilitiesObserver() override {}

  MOCK_METHOD1(OnCapabilitiesChanged, void(const std::string& path));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCapabilitiesObserver);
};

// Test fixtures needs an example default capability to test DeviceCapabilities
// methods. Gets a sample key and initial value.
void GetSampleDefaultCapability(std::string* key,
                                scoped_ptr<base::Value>* init_value) {
  DCHECK(key);
  DCHECK(init_value);
  *key = DeviceCapabilities::kKeyBluetoothSupported;
  *init_value = make_scoped_ptr(new base::FundamentalValue(true));
}

// For test fixtures that test dynamic capabilities, gets a sample key
// and initial value.
void GetSampleDynamicCapability(std::string* key,
                                scoped_ptr<base::Value>* init_value) {
  DCHECK(key);
  DCHECK(init_value);
  *key = "dummy_dynamic_key";
  *init_value = make_scoped_ptr(new base::FundamentalValue(99));
}

// Gets a value for sample default capability different from |init_value|
// returned in GetSampleDefaultCapability(). Must be of same type as
// |init_value| of course.
scoped_ptr<base::Value> GetSampleDefaultCapabilityNewValue() {
  return make_scoped_ptr(new base::FundamentalValue(false));
}

// Gets a value for sample dynamic capability different from |init_value|
// returned in GetSampleDynamicCapability(). Must be of same type as
// |init_value| of course.
scoped_ptr<base::Value> GetSampleDynamicCapabilityNewValue() {
  return make_scoped_ptr(new base::FundamentalValue(100));
}

// Tests that |json| string matches contents of a DictionaryValue with one entry
// specified by |key| and |value|.
bool JsonStringEquals(const std::string& json,
                      const std::string& key,
                      const base::Value& value) {
  base::DictionaryValue dict_value;
  dict_value.Set(key, value.CreateDeepCopy());
  scoped_ptr<const std::string> dict_json(SerializeToJson(dict_value));
  return dict_json.get() && *dict_json == json;
}

}  // namespace

class DeviceCapabilitiesImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    capabilities_ = DeviceCapabilities::Create();
    mock_capabilities_observer_.reset(new MockCapabilitiesObserver());
    capabilities_->AddCapabilitiesObserver(mock_capabilities_observer_.get());

    // We set the default gmock expected calls to any so that tests must
    // 'opt in' to checking the calls rather than 'opt out'. This avoids having
    // to add explicit calls in test cases that don't care in order to prevent
    // lots of useless mock warnings.
    EXPECT_CALL(*mock_capabilities_observer_, OnCapabilitiesChanged(testing::_))
        .Times(testing::AnyNumber());
  }

  void TearDown() override {
    capabilities_->RemoveCapabilitiesObserver(
        mock_capabilities_observer_.get());
  }

  DeviceCapabilities* capabilities() const { return capabilities_.get(); }

  MockCapabilitiesObserver* capabilities_observer() const {
    return mock_capabilities_observer_.get();
  }

 private:
  scoped_ptr<DeviceCapabilities> capabilities_;
  scoped_ptr<MockCapabilitiesObserver> mock_capabilities_observer_;
};

// Tests that class is in correct state after Create().
TEST_F(DeviceCapabilitiesImplTest, Create) {
  scoped_ptr<const std::string> empty_dict_string(
      SerializeToJson(base::DictionaryValue()));
  EXPECT_EQ(capabilities()->GetCapabilitiesString(), *empty_dict_string);
  EXPECT_TRUE(capabilities()->GetCapabilities()->empty());
}

// Tests Register() of a default capability.
TEST_F(DeviceCapabilitiesImplTest, Register) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, true);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(2);
  capabilities()->Register(key, init_value->CreateDeepCopy(), &manager);
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetCapabilitiesString(), key,
                               *init_value));
  const base::Value* dict_value;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(key, &dict_value));
  EXPECT_TRUE(base::Value::Equals(init_value.get(), dict_value));
  capabilities()->Unregister(key, &manager);
}

// Tests Unregister() of a default capability.
TEST_F(DeviceCapabilitiesImplTest, Unregister) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, true);

  capabilities()->Register(key, init_value->CreateDeepCopy(), &manager);

  capabilities()->Unregister(key, &manager);
  scoped_ptr<const std::string> empty_dict_string(
      SerializeToJson(base::DictionaryValue()));
  EXPECT_EQ(capabilities()->GetCapabilitiesString(), *empty_dict_string);
  EXPECT_TRUE(capabilities()->GetCapabilities()->empty());
}

// Tests GetCapability() and updating the value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, GetCapabilityAndSetCapability) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, true);

  capabilities()->Register(key, init_value->CreateDeepCopy(), &manager);

  const base::Value* value = nullptr;
  EXPECT_TRUE(capabilities()->GetCapability(key, &value));
  EXPECT_TRUE(base::Value::Equals(value, init_value.get()));

  scoped_ptr<base::Value> new_value = GetSampleDefaultCapabilityNewValue();
  capabilities()->SetCapability(key, new_value->CreateDeepCopy());
  value = nullptr;
  EXPECT_TRUE(capabilities()->GetCapability(key, &value));
  EXPECT_TRUE(base::Value::Equals(value, new_value.get()));

  capabilities()->Unregister(key, &manager);
  value = nullptr;
  EXPECT_FALSE(capabilities()->GetCapability(key, &value));
  EXPECT_EQ(value, nullptr);
}

// Tests BluetoothSupported() and updating this value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, BluetoothSupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyBluetoothSupported, true);

  capabilities()->Register(DeviceCapabilities::kKeyBluetoothSupported,
                           make_scoped_ptr(new base::FundamentalValue(true)),
                           &manager);

  EXPECT_TRUE(capabilities()->BluetoothSupported());
  capabilities()->SetCapability(
      DeviceCapabilities::kKeyBluetoothSupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  EXPECT_FALSE(capabilities()->BluetoothSupported());

  capabilities()->Unregister(DeviceCapabilities::kKeyBluetoothSupported,
                             &manager);
}

// Tests DisplaySupported() and updating this value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, DisplaySupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyDisplaySupported, true);

  capabilities()->Register(DeviceCapabilities::kKeyDisplaySupported,
                           make_scoped_ptr(new base::FundamentalValue(true)),
                           &manager);

  EXPECT_TRUE(capabilities()->DisplaySupported());
  capabilities()->SetCapability(
      DeviceCapabilities::kKeyDisplaySupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  EXPECT_FALSE(capabilities()->DisplaySupported());

  capabilities()->Unregister(DeviceCapabilities::kKeyDisplaySupported,
                             &manager);
}

// Tests SetCapability() for a default capability when the capability's manager
// rejects the proposed change.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityInvalid) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, false);

  capabilities()->Register(key, init_value->CreateDeepCopy(), &manager);

  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  const base::Value* dict_value = nullptr;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(key, &dict_value));
  EXPECT_TRUE(base::Value::Equals(init_value.get(), dict_value));

  capabilities()->Unregister(key, &manager);
}

// Test that SetCapability() updates the capabilities string correctly
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityUpdatesString) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, true);

  capabilities()->Register(key, init_value->CreateDeepCopy(), &manager);

  EXPECT_TRUE(JsonStringEquals(capabilities()->GetCapabilitiesString(), key,
                               *init_value));

  scoped_ptr<base::Value> new_value = GetSampleDefaultCapabilityNewValue();
  capabilities()->SetCapability(key, new_value->CreateDeepCopy());
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetCapabilitiesString(), key,
                               *new_value));

  capabilities()->Unregister(key, &manager);
}

// Test that SetCapability() notifies Observers when the capability's value
// changes
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityNotifiesObservers) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, true);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(4);

  capabilities()->Register(key, init_value->CreateDeepCopy(), &manager);

  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // Observer should not get called when value does not change
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  capabilities()->SetCapability(key, init_value.Pass());

  capabilities()->Unregister(key, &manager);
}

// Test adding dynamic capabilities
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDynamic) {
  std::string key;
  scoped_ptr<base::Value> init_value;

  ASSERT_FALSE(capabilities()->GetCapabilities()->HasKey(key));

  GetSampleDynamicCapability(&key, &init_value);
  capabilities()->SetCapability(key, init_value->CreateDeepCopy());

  const base::Value* dict_value = nullptr;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(key, &dict_value));
  EXPECT_TRUE(base::Value::Equals(init_value.get(), dict_value));
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetCapabilitiesString(), key,
                               *init_value));

  scoped_ptr<base::Value> new_value = GetSampleDynamicCapabilityNewValue();
  capabilities()->SetCapability(key, new_value->CreateDeepCopy());
  dict_value = nullptr;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(key, &dict_value));
  EXPECT_TRUE(base::Value::Equals(new_value.get(), dict_value));
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetCapabilitiesString(), key,
                               *new_value));
}

// Tests that SetCapability() works with expanded paths when there is a
// capability of type DictionaryValue.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDictionary) {
  std::string key("dummy_dictionary_key");
  scoped_ptr<base::Value> init_value =
      DeserializeFromJson(kSampleDictionaryCapability);
  ASSERT_NE(init_value, nullptr);
  FakeCapabilityManagerSimple manager(capabilities(), key, true);

  capabilities()->Register(key, init_value.Pass(), &manager);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_bool",
      make_scoped_ptr(new base::FundamentalValue(false)));
  const base::Value* dict_value = nullptr;
  bool dict_value_bool = true;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(
      "dummy_dictionary_key.dummy_field_bool", &dict_value));
  EXPECT_TRUE(dict_value->GetAsBoolean(&dict_value_bool));
  EXPECT_FALSE(dict_value_bool);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_int",
      make_scoped_ptr(new base::FundamentalValue(100)));
  dict_value = nullptr;
  int dict_value_int = 0;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(
      "dummy_dictionary_key.dummy_field_int", &dict_value));
  EXPECT_TRUE(dict_value->GetAsInteger(&dict_value_int));
  EXPECT_EQ(dict_value_int, 100);

  capabilities()->Unregister(key, &manager);
}

// Tests that SetCapability() works with expanded paths when there is a
// capability of type DictionaryValue and invalid changes are proposed. Ensures
// that the manager's ValidationCallback is getting called for expanded paths
// passed to SetCapability().
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDictionaryInvalid) {
  std::string key("dummy_dictionary_key");
  scoped_ptr<base::Value> init_value =
      DeserializeFromJson(kSampleDictionaryCapability);
  ASSERT_NE(init_value, nullptr);
  FakeCapabilityManagerSimple manager(capabilities(), key, false);

  capabilities()->Register(key, init_value.Pass(), &manager);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_bool",
      make_scoped_ptr(new base::FundamentalValue(false)));
  const base::Value* dict_value = nullptr;
  bool dict_value_bool = false;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(
      "dummy_dictionary_key.dummy_field_bool", &dict_value));
  EXPECT_TRUE(dict_value->GetAsBoolean(&dict_value_bool));
  EXPECT_TRUE(dict_value_bool);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_int",
      make_scoped_ptr(new base::FundamentalValue(100)));
  dict_value = nullptr;
  int dict_value_int = 0;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get(
      "dummy_dictionary_key.dummy_field_int", &dict_value));
  EXPECT_TRUE(dict_value->GetAsInteger(&dict_value_int));
  EXPECT_EQ(dict_value_int, 99);

  capabilities()->Unregister(key, &manager);
}

// Test  MergeDictionary.
TEST_F(DeviceCapabilitiesImplTest, MergeDictionary) {
  scoped_ptr<base::Value> value =
      DeserializeFromJson(kSampleDictionaryCapability);
  ASSERT_NE(value, nullptr);
  base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));
  ASSERT_NE(dict_value, nullptr);

  capabilities()->MergeDictionary(*dict_value);

  // First make sure that capabilities get created if they do not exist
  const base::Value* internal_value = nullptr;
  bool dict_value_bool = false;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get("dummy_field_bool",
                                                     &internal_value));
  EXPECT_TRUE(internal_value->GetAsBoolean(&dict_value_bool));
  EXPECT_TRUE(dict_value_bool);

  internal_value = nullptr;
  int dict_value_int = 0;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get("dummy_field_int",
                                                     &internal_value));
  EXPECT_TRUE(internal_value->GetAsInteger(&dict_value_int));
  EXPECT_EQ(dict_value_int, 99);

  // Now just update one of the fields. Make sure the updated value is changed
  // in DeviceCapabilities and the other field remains untouched.
  dict_value->SetInteger("dummy_field_int", 100);
  ASSERT_TRUE(dict_value->Remove("dummy_field_bool", nullptr));

  capabilities()->MergeDictionary(*dict_value);

  internal_value = nullptr;
  dict_value_bool = false;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get("dummy_field_bool",
                                                     &internal_value));
  EXPECT_TRUE(internal_value->GetAsBoolean(&dict_value_bool));
  EXPECT_TRUE(dict_value_bool);

  internal_value = nullptr;
  EXPECT_TRUE(capabilities()->GetCapabilities()->Get("dummy_field_int",
                                                     &internal_value));
  EXPECT_TRUE(internal_value->GetAsInteger(&dict_value_int));
  EXPECT_EQ(dict_value_int, 100);
}

}  // namespace chromecast
