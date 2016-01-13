// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/device_capabilities_impl.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
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

void GetSampleDefaultCapability(std::string* key,
                                scoped_ptr<base::Value>* init_value);
void TestBasicOperations(DeviceCapabilities* capabilities);

// Simple capability manager that implements the Validator interface. Either
// accepts or rejects all proposed changes based on |accept_changes| constructor
// argument.
class FakeCapabilityManagerSimple : public DeviceCapabilities::Validator {
 public:
  // Registers itself as Validator in constructor. If init_value is not null,
  // the capability gets initialized to that value. Else capability remains
  // untouched.
  FakeCapabilityManagerSimple(DeviceCapabilities* capabilities,
                              const std::string& key,
                              scoped_ptr<base::Value> init_value,
                              bool accept_changes)
      : DeviceCapabilities::Validator(capabilities),
        key_(key),
        accept_changes_(accept_changes) {
    capabilities->Register(key, this);
    if (init_value)
      SetValidatedValue(key, std::move(init_value));
  }

  // Unregisters itself as Validator.
  ~FakeCapabilityManagerSimple() override {
    capabilities()->Unregister(key_, this);
  }

  void Validate(const std::string& path,
                scoped_ptr<base::Value> proposed_value) override {
    ASSERT_TRUE(path.find(key_) == 0);
    if (accept_changes_)
      SetValidatedValue(path, std::move(proposed_value));
  }

 private:
  const std::string key_;
  const bool accept_changes_;
};

// Used to test that capabilities/validator can be read and written in
// Validate() without encountering deadlocks/unexpected behavior.
class FakeCapabilityManagerComplex : public DeviceCapabilities::Validator {
 public:
  FakeCapabilityManagerComplex(DeviceCapabilities* capabilities,
                               const std::string& key)
      : DeviceCapabilities::Validator(capabilities), key_(key) {
    capabilities->Register(key, this);
  }

  // Unregisters itself as Validator.
  ~FakeCapabilityManagerComplex() override {
    capabilities()->Unregister(key_, this);
  }

  // Runs TestBasicOperations().
  void Validate(const std::string& path,
                scoped_ptr<base::Value> proposed_value) override {
    TestBasicOperations(capabilities());
  }

 private:
  const std::string key_;
};

// Used to test that capabilities/validators can be read and written in
// OnCapabilitiesChanged() without encountering deadlocks/unexpected behavior.
class FakeCapabilitiesObserver : public DeviceCapabilities::Observer {
 public:
  FakeCapabilitiesObserver(DeviceCapabilities* capabilities)
      : capabilities_(capabilities), removed_as_observer(false) {
    capabilities_->AddCapabilitiesObserver(this);
  }

  ~FakeCapabilitiesObserver() override {
    if (!removed_as_observer)
      capabilities_->RemoveCapabilitiesObserver(this);
  }

  // Runs TestBasicOperations().
  void OnCapabilitiesChanged(const std::string& path) override {
    TestBasicOperations(capabilities_);
    // To prevent infinite loop of SetCapability() -> OnCapabilitiesChanged()
    // -> SetCapability() -> OnCapabilitiesChanged() etc.
    capabilities_->RemoveCapabilitiesObserver(this);
    removed_as_observer = true;
  }

 private:
  DeviceCapabilities* const capabilities_;
  bool removed_as_observer;
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

// The function runs through the set of basic operations of DeviceCapabilities.
// Register validator for sample default capability, reads capability, writes
// capability, and unregister validator. After it has completed, use
// AssertBasicOperationsSuccessful() to ensure that all operations completed
// successfully. Sample default capability should not be added or registered in
// class before this function is called.
void TestBasicOperations(DeviceCapabilities* capabilities) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);

  ASSERT_FALSE(capabilities->GetCapability(key));
  ASSERT_FALSE(capabilities->GetValidator(key));

  // Register and write capability
  FakeCapabilityManagerSimple* manager(new FakeCapabilityManagerSimple(
      capabilities, key, init_value->CreateDeepCopy(), true));
  // Read Validator
  EXPECT_EQ(capabilities->GetValidator(key), manager);
  // Read Capability
  EXPECT_TRUE(base::Value::Equals(capabilities->GetCapability(key).get(),
                                  init_value.get()));
  // Unregister
  delete manager;

  // Write capability again. Provides way of checking that this function
  // ran and was successful.
  scoped_ptr<base::Value> new_value = GetSampleDefaultCapabilityNewValue();
  capabilities->SetCapability(key, std::move(new_value));
}

// See TestBasicOperations() comment.
void AssertBasicOperationsSuccessful(const DeviceCapabilities* capabilities) {
  base::RunLoop().RunUntilIdle();
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  scoped_ptr<base::Value> value = capabilities->GetCapability(key);
  ASSERT_TRUE(value);
  scoped_ptr<base::Value> new_value = GetSampleDefaultCapabilityNewValue();
  EXPECT_TRUE(base::Value::Equals(value.get(), new_value.get()));
}

}  // namespace

class DeviceCapabilitiesImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));
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
    mock_capabilities_observer_.reset();
    capabilities_.reset();
    message_loop_.reset();
  }

  DeviceCapabilities* capabilities() const { return capabilities_.get(); }

  MockCapabilitiesObserver* capabilities_observer() const {
    return mock_capabilities_observer_.get();
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<DeviceCapabilities> capabilities_;
  scoped_ptr<MockCapabilitiesObserver> mock_capabilities_observer_;
};

// Tests that class is in correct state after Create().
TEST_F(DeviceCapabilitiesImplTest, Create) {
  scoped_ptr<const std::string> empty_dict_string(
      SerializeToJson(base::DictionaryValue()));
  EXPECT_EQ(capabilities()->GetData()->json_string(), *empty_dict_string);
  EXPECT_TRUE(capabilities()->GetData()->dictionary().empty());
}

// Tests Register() of a default capability.
TEST_F(DeviceCapabilitiesImplTest, Register) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(0);
  FakeCapabilityManagerSimple manager(capabilities(), key, nullptr, true);

  EXPECT_EQ(capabilities()->GetValidator(key), &manager);
  scoped_ptr<const std::string> empty_dict_string(
      SerializeToJson(base::DictionaryValue()));
  EXPECT_EQ(capabilities()->GetData()->json_string(), *empty_dict_string);
  EXPECT_FALSE(capabilities()->GetCapability(key));
}

// Tests Unregister() of a default capability.
TEST_F(DeviceCapabilitiesImplTest, Unregister) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(0);
  FakeCapabilityManagerSimple* manager =
      new FakeCapabilityManagerSimple(capabilities(), key, nullptr, true);

  delete manager;

  EXPECT_FALSE(capabilities()->GetValidator(key));
  scoped_ptr<const std::string> empty_dict_string(
      SerializeToJson(base::DictionaryValue()));
  EXPECT_EQ(capabilities()->GetData()->json_string(), *empty_dict_string);
  EXPECT_FALSE(capabilities()->GetCapability(key));
}

// Tests GetCapability() and updating the value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, GetCapabilityAndSetCapability) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      init_value->CreateDeepCopy(), true);

  EXPECT_TRUE(base::Value::Equals(capabilities()->GetCapability(key).get(),
                                  init_value.get()));

  scoped_ptr<base::Value> new_value = GetSampleDefaultCapabilityNewValue();
  capabilities()->SetCapability(key, new_value->CreateDeepCopy());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::Value::Equals(capabilities()->GetCapability(key).get(),
                                  new_value.get()));
}

// Tests BluetoothSupported() and updating this value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, BluetoothSupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyBluetoothSupported,
      make_scoped_ptr(new base::FundamentalValue(true)), true);

  EXPECT_TRUE(capabilities()->BluetoothSupported());
  capabilities()->SetCapability(
      DeviceCapabilities::kKeyBluetoothSupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->BluetoothSupported());
}

// Tests DisplaySupported() and updating this value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, DisplaySupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyDisplaySupported,
      make_scoped_ptr(new base::FundamentalValue(true)), true);

  EXPECT_TRUE(capabilities()->DisplaySupported());
  capabilities()->SetCapability(
      DeviceCapabilities::kKeyDisplaySupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->DisplaySupported());
}

// Tests HiResAudioSupported() and updating this value through SetCapability()
TEST_F(DeviceCapabilitiesImplTest, HiResAudioSupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyHiResAudioSupported,
      make_scoped_ptr(new base::FundamentalValue(true)), true);

  EXPECT_TRUE(capabilities()->HiResAudioSupported());
  capabilities()->SetCapability(
      DeviceCapabilities::kKeyHiResAudioSupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->HiResAudioSupported());
}

// Tests SetCapability() for a default capability when the capability's manager
// rejects the proposed change.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityInvalid) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      init_value->CreateDeepCopy(), false);

  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::Value::Equals(capabilities()->GetCapability(key).get(),
                                  init_value.get()));
}

// Test that SetCapability() updates the capabilities string correctly
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityUpdatesString) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      init_value->CreateDeepCopy(), true);

  EXPECT_TRUE(JsonStringEquals(capabilities()->GetData()->json_string(), key,
                               *init_value));

  scoped_ptr<base::Value> new_value = GetSampleDefaultCapabilityNewValue();
  capabilities()->SetCapability(key, new_value->CreateDeepCopy());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetData()->json_string(), key,
                               *new_value));
}

// Test that SetCapability() notifies Observers when the capability's value
// changes
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityNotifiesObservers) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(3);

  // 1st call (register)
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      init_value->CreateDeepCopy(), true);

  // 2nd call
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // Observer should not get called when value does not change
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // 3rd call
  capabilities()->SetCapability(key, std::move(init_value));
  base::RunLoop().RunUntilIdle();
}

// Test adding dynamic capabilities
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDynamic) {
  std::string key;
  scoped_ptr<base::Value> init_value;
  GetSampleDynamicCapability(&key, &init_value);

  ASSERT_FALSE(capabilities()->GetCapability(key));
  capabilities()->SetCapability(key, init_value->CreateDeepCopy());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::Value::Equals(capabilities()->GetCapability(key).get(),
                                  init_value.get()));
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetData()->json_string(), key,
                               *init_value));

  scoped_ptr<base::Value> new_value = GetSampleDynamicCapabilityNewValue();
  capabilities()->SetCapability(key, new_value->CreateDeepCopy());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::Value::Equals(capabilities()->GetCapability(key).get(),
                                  new_value.get()));
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetData()->json_string(), key,
                               *new_value));
}

// Tests that SetCapability() works with expanded paths when there is a
// capability of type DictionaryValue.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDictionary) {
  std::string key("dummy_dictionary_key");
  scoped_ptr<base::Value> init_value =
      DeserializeFromJson(kSampleDictionaryCapability);
  ASSERT_TRUE(init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      std::move(init_value), true);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_bool",
      make_scoped_ptr(new base::FundamentalValue(false)));
  base::RunLoop().RunUntilIdle();
  bool value_bool = true;
  scoped_ptr<base::Value> value =
      capabilities()->GetCapability("dummy_dictionary_key.dummy_field_bool");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&value_bool));
  EXPECT_FALSE(value_bool);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_int",
      make_scoped_ptr(new base::FundamentalValue(100)));
  base::RunLoop().RunUntilIdle();
  int value_int = 0;
  value = capabilities()->GetCapability("dummy_dictionary_key.dummy_field_int");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&value_int));
  EXPECT_EQ(value_int, 100);
}

// Tests that SetCapability() works with expanded paths when there is a
// capability of type DictionaryValue and invalid changes are proposed.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDictionaryInvalid) {
  std::string key("dummy_dictionary_key");
  scoped_ptr<base::Value> init_value =
      DeserializeFromJson(kSampleDictionaryCapability);
  ASSERT_TRUE(init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      std::move(init_value), false);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_bool",
      make_scoped_ptr(new base::FundamentalValue(false)));
  base::RunLoop().RunUntilIdle();
  bool value_bool = false;
  scoped_ptr<base::Value> value =
      capabilities()->GetCapability("dummy_dictionary_key.dummy_field_bool");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&value_bool));
  EXPECT_TRUE(value_bool);

  capabilities()->SetCapability(
      "dummy_dictionary_key.dummy_field_int",
      make_scoped_ptr(new base::FundamentalValue(100)));
  base::RunLoop().RunUntilIdle();
  int value_int = 0;
  value = capabilities()->GetCapability("dummy_dictionary_key.dummy_field_int");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&value_int));
  EXPECT_EQ(value_int, 99);
}

// Test  MergeDictionary.
TEST_F(DeviceCapabilitiesImplTest, MergeDictionary) {
  scoped_ptr<base::Value> deserialized_value =
      DeserializeFromJson(kSampleDictionaryCapability);
  ASSERT_TRUE(deserialized_value);
  base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(deserialized_value->GetAsDictionary(&dict_value));
  ASSERT_TRUE(dict_value);

  capabilities()->MergeDictionary(*dict_value);
  base::RunLoop().RunUntilIdle();

  // First make sure that capabilities get created if they do not exist
  bool value_bool = false;
  scoped_ptr<base::Value> value =
      capabilities()->GetCapability("dummy_field_bool");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&value_bool));
  EXPECT_TRUE(value_bool);

  int value_int = 0;
  value = capabilities()->GetCapability("dummy_field_int");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&value_int));
  EXPECT_EQ(value_int, 99);

  // Now just update one of the fields. Make sure the updated value is changed
  // in DeviceCapabilities and the other field remains untouched.
  dict_value->SetInteger("dummy_field_int", 100);
  ASSERT_TRUE(dict_value->Remove("dummy_field_bool", nullptr));

  capabilities()->MergeDictionary(*dict_value);
  base::RunLoop().RunUntilIdle();

  value_bool = false;
  value = capabilities()->GetCapability("dummy_field_bool");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&value_bool));
  EXPECT_TRUE(value_bool);

  value = capabilities()->GetCapability("dummy_field_int");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&value_int));
  EXPECT_EQ(value_int, 100);
}

// Tests that it is safe to call DeviceCapabilities methods in
// an Observer's OnCapabilitiesChanged() implementation safely with correct
// behavior and without deadlocking.
TEST_F(DeviceCapabilitiesImplTest, OnCapabilitiesChangedSafe) {
  FakeCapabilitiesObserver observer(capabilities());

  // Trigger FakeCapabilitiesObserver::OnCapabilitiesChanged()
  capabilities()->SetCapability(
      "dummy_trigger_key", make_scoped_ptr(new base::FundamentalValue(true)));
  base::RunLoop().RunUntilIdle();

  // Check that FakeCapabilitiesObserver::OnCapabilitiesChanged() ran and that
  // behavior was successful
  AssertBasicOperationsSuccessful(capabilities());
}

// Tests that it is safe to call DeviceCapabilities methods in a Validator's
// Validate() implementation safely with correct behavior and without
// deadlocking.
TEST_F(DeviceCapabilitiesImplTest, ValidateSafe) {
  FakeCapabilityManagerComplex manager(capabilities(), "dummy_validate_key");

  // Trigger FakeCapabilityManagerComplex::Validate()
  capabilities()->SetCapability(
      "dummy_validate_key", make_scoped_ptr(new base::FundamentalValue(true)));
  base::RunLoop().RunUntilIdle();

  // Check that FakeCapabilityManagerComplex::Validate() ran and that behavior
  // was successful
  AssertBasicOperationsSuccessful(capabilities());
}

}  // namespace chromecast
