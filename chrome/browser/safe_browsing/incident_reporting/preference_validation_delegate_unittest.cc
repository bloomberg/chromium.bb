// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/preference_validation_delegate.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::WithArg;

// A basic test harness that creates a delegate instance for which it stores all
// incidents. Tests can push data to the delegate and verify that the test
// instance was provided with the expected data.
class PreferenceValidationDelegateTest : public testing::Test {
 protected:
  typedef std::vector<std::unique_ptr<safe_browsing::Incident>> IncidentVector;

  PreferenceValidationDelegateTest()
      : kPrefPath_("atomic.pref"),
        null_value_(base::Value::CreateNullValue()) {}

  void SetUp() override {
    testing::Test::SetUp();
    invalid_keys_.push_back(std::string("one"));
    invalid_keys_.push_back(std::string("two"));
    external_validation_invalid_keys_.push_back(std::string("three"));
    std::unique_ptr<safe_browsing::MockIncidentReceiver> receiver(
        new NiceMock<safe_browsing::MockIncidentReceiver>());
    ON_CALL(*receiver, DoAddIncidentForProfile(IsNull(), _))
        .WillByDefault(WithArg<1>(TakeIncidentToVector(&incidents_)));
    instance_.reset(new safe_browsing::PreferenceValidationDelegate(
        nullptr, std::move(receiver)));
  }

  static void ExpectValueStatesEquate(
      PrefHashStoreTransaction::ValueState store_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      safe_browsing::
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident_ValueState
              incident_state) {
    typedef safe_browsing::
        ClientIncidentReport_IncidentData_TrackedPreferenceIncident TPIncident;
    switch (store_state) {
      case PrefHashStoreTransaction::CLEARED:
        EXPECT_EQ(TPIncident::CLEARED, incident_state);
        break;
      case PrefHashStoreTransaction::CHANGED:
        EXPECT_EQ(TPIncident::CHANGED, incident_state);
        break;
      case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:
        EXPECT_EQ(TPIncident::UNTRUSTED_UNKNOWN_VALUE, incident_state);
        break;
      default:
        if (external_validation_value_state ==
            PrefHashStoreTransaction::CLEARED) {
          EXPECT_EQ(TPIncident::BYPASS_CLEARED, incident_state);
        } else if (external_validation_value_state ==
                   PrefHashStoreTransaction::CHANGED) {
          EXPECT_EQ(TPIncident::BYPASS_CHANGED, incident_state);
        } else {
          FAIL() << "unexpected store state";
        }
        break;
    }
  }

  static void ExpectKeysEquate(
      const std::vector<std::string>& store_keys,
      const google::protobuf::RepeatedPtrField<std::string>& incident_keys) {
    ASSERT_EQ(store_keys.size(), static_cast<size_t>(incident_keys.size()));
    for (int i = 0; i < incident_keys.size(); ++i) {
      EXPECT_EQ(store_keys[i], incident_keys.Get(i));
    }
  }

  const std::string kPrefPath_;
  IncidentVector incidents_;
  std::unique_ptr<base::Value> null_value_;
  base::DictionaryValue dict_value_;
  std::vector<std::string> invalid_keys_;
  std::vector<std::string> external_validation_invalid_keys_;
  std::unique_ptr<TrackedPreferenceValidationDelegate> instance_;
};

// Tests that a NULL value results in an incident with no value.
TEST_F(PreferenceValidationDelegateTest, NullValue) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath_, NULL, PrefHashStoreTransaction::CLEARED,
      PrefHashStoreTransaction::UNSUPPORTED, false /* is_personal */);
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_FALSE(incident->tracked_preference().has_atomic_value());
  EXPECT_EQ(
      safe_browsing::
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident::CLEARED,
      incident->tracked_preference().value_state());
}

// Tests that all supported value types can be stringified into an incident. The
// parameters for the test are the type of value to test and the expected value
// string.
class PreferenceValidationDelegateValues
    : public PreferenceValidationDelegateTest,
      public testing::WithParamInterface<
          std::tr1::tuple<base::Value::Type, const char*> > {
 protected:
  void SetUp() override {
    PreferenceValidationDelegateTest::SetUp();
    value_type_ = std::tr1::get<0>(GetParam());
    expected_value_ = std::tr1::get<1>(GetParam());
  }

  static std::unique_ptr<base::Value> MakeValue(base::Value::Type value_type) {
    using base::Value;
    switch (value_type) {
      case Value::Type::NONE:
        return Value::CreateNullValue();
      case Value::Type::BOOLEAN:
        return std::unique_ptr<Value>(new base::Value(false));
      case Value::Type::INTEGER:
        return std::unique_ptr<Value>(new base::Value(47));
      case Value::Type::DOUBLE:
        return std::unique_ptr<Value>(new base::Value(0.47));
      case Value::Type::STRING:
        return std::unique_ptr<Value>(new base::StringValue("i have a spleen"));
      case Value::Type::DICTIONARY: {
        std::unique_ptr<base::DictionaryValue> value(
            new base::DictionaryValue());
        value->SetInteger("twenty-two", 22);
        value->SetInteger("forty-seven", 47);
        return std::move(value);
      }
      case Value::Type::LIST: {
        std::unique_ptr<base::ListValue> value(new base::ListValue());
        value->AppendInteger(22);
        value->AppendInteger(47);
        return std::move(value);
      }
      default:
        ADD_FAILURE() << "unsupported value type " << value_type;
    }
    return std::unique_ptr<Value>();
  }

  base::Value::Type value_type_;
  const char* expected_value_;
};

TEST_P(PreferenceValidationDelegateValues, Value) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath_, MakeValue(value_type_).get(),
      PrefHashStoreTransaction::CLEARED, PrefHashStoreTransaction::UNSUPPORTED,
      false /* is_personal */);
  ASSERT_EQ(1U, incidents_.size());
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_EQ(std::string(expected_value_),
            incident->tracked_preference().atomic_value());
}

INSTANTIATE_TEST_CASE_P(
    Values,
    PreferenceValidationDelegateValues,
    // On Android, make_tuple(..., "null") doesn't compile due to the error:
    // testing/gtest/include/gtest/internal/gtest-tuple.h:246:48:
    //   error: array used as initializer
    testing::Values(
        std::tr1::make_tuple(base::Value::Type::NONE,
                             const_cast<char*>("null")),
        std::tr1::make_tuple(base::Value::Type::BOOLEAN,
                             const_cast<char*>("false")),
        std::tr1::make_tuple(base::Value::Type::INTEGER,
                             const_cast<char*>("47")),
        std::tr1::make_tuple(base::Value::Type::DOUBLE,
                             const_cast<char*>("0.47")),
        std::tr1::make_tuple(base::Value::Type::STRING,
                             const_cast<char*>("i have a spleen")),
        std::tr1::make_tuple(base::Value::Type::DICTIONARY,
            const_cast<char*>("{\"forty-seven\":47,\"twenty-two\":22}")),
        std::tr1::make_tuple(base::Value::Type::LIST,
                             const_cast<char*>("[22,47]"))));

// Tests that no incidents are reported for relevant combinations of ValueState.
class PreferenceValidationDelegateNoIncident
    : public PreferenceValidationDelegateTest,
      public testing::WithParamInterface<
          std::tr1::tuple<PrefHashStoreTransaction::ValueState,
                          PrefHashStoreTransaction::ValueState>> {
 protected:
  void SetUp() override {
    PreferenceValidationDelegateTest::SetUp();
    value_state_ = std::tr1::get<0>(GetParam());
    external_validation_value_state_ = std::tr1::get<1>(GetParam());
  }

  PrefHashStoreTransaction::ValueState value_state_;
  PrefHashStoreTransaction::ValueState external_validation_value_state_;
};

TEST_P(PreferenceValidationDelegateNoIncident, Atomic) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath_, null_value_.get(), value_state_,
      external_validation_value_state_, false /* is_personal */);
  EXPECT_EQ(0U, incidents_.size());
}

TEST_P(PreferenceValidationDelegateNoIncident, Split) {
  instance_->OnSplitPreferenceValidation(
      kPrefPath_, &dict_value_, invalid_keys_,
      external_validation_invalid_keys_, value_state_,
      external_validation_value_state_, false /* is_personal */);
  EXPECT_EQ(0U, incidents_.size());
}

INSTANTIATE_TEST_CASE_P(
    NoIncident,
    PreferenceValidationDelegateNoIncident,
    testing::Combine(
        testing::Values(PrefHashStoreTransaction::UNCHANGED,
                        PrefHashStoreTransaction::SECURE_LEGACY,
                        PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE),
        testing::Values(PrefHashStoreTransaction::UNCHANGED,
                        PrefHashStoreTransaction::UNSUPPORTED,
                        PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE)));

// Tests that incidents are reported for relevant combinations of ValueState and
// impersonal/personal.
class PreferenceValidationDelegateWithIncident
    : public PreferenceValidationDelegateTest,
      public testing::WithParamInterface<
          std::tr1::tuple<PrefHashStoreTransaction::ValueState,
                          PrefHashStoreTransaction::ValueState,
                          bool>> {
 protected:
  void SetUp() override {
    PreferenceValidationDelegateTest::SetUp();
    value_state_ = std::tr1::get<0>(GetParam());
    external_validation_value_state_ = std::tr1::get<1>(GetParam());
    is_personal_ = std::tr1::get<2>(GetParam());
  }

  PrefHashStoreTransaction::ValueState value_state_;
  PrefHashStoreTransaction::ValueState external_validation_value_state_;
  bool is_personal_;
};

TEST_P(PreferenceValidationDelegateWithIncident, Atomic) {
  instance_->OnAtomicPreferenceValidation(
      kPrefPath_, null_value_.get(), value_state_,
      external_validation_value_state_, is_personal_);
  ASSERT_EQ(1U, incidents_.size());
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_TRUE(incident->has_tracked_preference());
  const safe_browsing::
      ClientIncidentReport_IncidentData_TrackedPreferenceIncident& tp_incident =
          incident->tracked_preference();
  EXPECT_EQ(kPrefPath_, tp_incident.path());
  EXPECT_EQ(0, tp_incident.split_key_size());
  if (!is_personal_) {
    EXPECT_TRUE(tp_incident.has_atomic_value());
    EXPECT_EQ(std::string("null"), tp_incident.atomic_value());
  } else {
    EXPECT_FALSE(tp_incident.has_atomic_value());
  }
  EXPECT_TRUE(tp_incident.has_value_state());
  ExpectValueStatesEquate(value_state_, external_validation_value_state_,
                          tp_incident.value_state());
}

TEST_P(PreferenceValidationDelegateWithIncident, Split) {
  instance_->OnSplitPreferenceValidation(
      kPrefPath_, &dict_value_, invalid_keys_,
      external_validation_invalid_keys_, value_state_,
      external_validation_value_state_, is_personal_);
  ASSERT_EQ(1U, incidents_.size());
  std::unique_ptr<safe_browsing::ClientIncidentReport_IncidentData> incident(
      incidents_.back()->TakePayload());
  EXPECT_TRUE(incident->has_tracked_preference());
  const safe_browsing::
      ClientIncidentReport_IncidentData_TrackedPreferenceIncident& tp_incident =
          incident->tracked_preference();
  EXPECT_EQ(kPrefPath_, tp_incident.path());
  EXPECT_FALSE(tp_incident.has_atomic_value());
  if (!is_personal_) {
    if (value_state_ == PrefHashStoreTransaction::CLEARED ||
        value_state_ == PrefHashStoreTransaction::CHANGED ||
        value_state_ == PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE) {
      ExpectKeysEquate(invalid_keys_, tp_incident.split_key());
    } else {
      ExpectKeysEquate(external_validation_invalid_keys_,
                       tp_incident.split_key());
    }
  } else {
    EXPECT_EQ(0, tp_incident.split_key_size());
  }
  EXPECT_TRUE(tp_incident.has_value_state());
  ExpectValueStatesEquate(value_state_, external_validation_value_state_,
                          tp_incident.value_state());
}

INSTANTIATE_TEST_CASE_P(
    WithIncident,
    PreferenceValidationDelegateWithIncident,
    testing::Combine(
        testing::Values(PrefHashStoreTransaction::CLEARED,
                        PrefHashStoreTransaction::CHANGED,
                        PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE),
        testing::Values(PrefHashStoreTransaction::UNCHANGED,
                        PrefHashStoreTransaction::UNSUPPORTED,
                        PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE),
        testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    WithBypassIncident,
    PreferenceValidationDelegateWithIncident,
    testing::Combine(
        testing::Values(PrefHashStoreTransaction::UNCHANGED,
                        PrefHashStoreTransaction::SECURE_LEGACY,
                        PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE),
        testing::Values(PrefHashStoreTransaction::CHANGED,
                        PrefHashStoreTransaction::CLEARED),
        testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    WithIncidentIgnoreBypass,
    PreferenceValidationDelegateWithIncident,
    testing::Combine(
        testing::Values(PrefHashStoreTransaction::CLEARED,
                        PrefHashStoreTransaction::CHANGED,
                        PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE),
        testing::Values(PrefHashStoreTransaction::CHANGED,
                        PrefHashStoreTransaction::CLEARED),
        testing::Bool()));
