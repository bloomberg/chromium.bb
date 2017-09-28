// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_normalizer_impl.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {
namespace {

using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::TestdataSource;

const char kLocale[] = "US";

// The requester of normalization for this test.
class NormalizationDelegate : public AddressNormalizer::Delegate {
 public:
  NormalizationDelegate()
      : normalized_called_(false), not_normalized_called_(false) {}

  ~NormalizationDelegate() override {}

  void OnAddressNormalized(const AutofillProfile& profile) override {
    normalized_called_ = true;
    profile_ = profile;
  }

  void OnCouldNotNormalize(const AutofillProfile& profile) override {
    not_normalized_called_ = true;
    profile_ = profile;
  }

  bool normalized_called() const { return normalized_called_; }

  bool not_normalized_called() const { return not_normalized_called_; }

  const AutofillProfile& profile() const { return profile_; }

 private:
  bool normalized_called_;
  bool not_normalized_called_;
  AutofillProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(NormalizationDelegate);
};

// Used to load region rules for this test.
class ChromiumTestdataSource : public TestdataSource {
 public:
  ChromiumTestdataSource() : TestdataSource(true) {}

  ~ChromiumTestdataSource() override {}

  // For this test, only load the rules for the kLocale.
  void Get(const std::string& key, const Callback& data_ready) const override {
    data_ready(
        true, key,
        new std::string("{\"data/US\": "
                        "{\"id\":\"data/US\",\"key\":\"US\",\"name\":\"UNITED "
                        "STATES\",\"lang\":\"en\",\"languages\":\"en\"}}"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumTestdataSource);
};

// A test subclass of the AddressNormalizerImpl. Used to simulate rules not
// being loaded.
class TestAddressNormalizer : public AddressNormalizerImpl {
 public:
  TestAddressNormalizer(std::unique_ptr<::i18n::addressinput::Source> source,
                        std::unique_ptr<::i18n::addressinput::Storage> storage)
      : AddressNormalizerImpl(std::move(source), std::move(storage)),
        should_load_rules_(true) {}

  ~TestAddressNormalizer() override {}

  void ShouldLoadRules(bool should_load_rules) {
    should_load_rules_ = should_load_rules;
  }

  void LoadRulesForRegion(const std::string& region_code) override {
    if (should_load_rules_) {
      AddressNormalizerImpl::LoadRulesForRegion(region_code);
    }
  }

 private:
  bool should_load_rules_;

  DISALLOW_COPY_AND_ASSIGN(TestAddressNormalizer);
};

}  // namespace

class AddressNormalizerTest : public testing::Test {
 protected:
  AddressNormalizerTest()
      : normalizer_(new TestAddressNormalizer(
            std::unique_ptr<Source>(new ChromiumTestdataSource),
            std::unique_ptr<Storage>(new NullStorage))) {}

  ~AddressNormalizerTest() override {}

  const std::unique_ptr<TestAddressNormalizer> normalizer_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressNormalizerTest);
};

// Tests that rules are not loaded by default.
TEST_F(AddressNormalizerTest, AreRulesLoadedForRegion_NotLoaded) {
  EXPECT_FALSE(normalizer_->AreRulesLoadedForRegion(kLocale));
}

// Tests that the rules are loaded correctly.
TEST_F(AddressNormalizerTest, AreRulesLoadedForRegion_Loaded) {
  normalizer_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(normalizer_->AreRulesLoadedForRegion(kLocale));
}

// Tests that if the rules are loaded before the normalization is started, the
// normalized profile will be returned to the delegate synchronously.
TEST_F(AddressNormalizerTest, StartNormalization_RulesLoaded) {
  NormalizationDelegate delegate;
  AutofillProfile profile;

  // Load the rules.
  normalizer_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(normalizer_->AreRulesLoadedForRegion(kLocale));

  // Start the normalization.
  normalizer_->StartAddressNormalization(profile, kLocale, 0, &delegate);

  // Since the rules are already loaded, the address should be normalized
  // synchronously.
  EXPECT_TRUE(delegate.normalized_called());
  EXPECT_FALSE(delegate.not_normalized_called());
}

// Tests that if the rules are not loaded before the normalization and cannot be
// loaded after, the address will not be normalized and the delegate will be
// notified.
TEST_F(AddressNormalizerTest, StartNormalization_RulesNotLoaded_WillNotLoad) {
  NormalizationDelegate delegate;
  AutofillProfile profile;

  // Make sure the rules will not be loaded in the StartAddressNormalization
  // call.
  normalizer_->ShouldLoadRules(false);

  // Start the normalization.
  normalizer_->StartAddressNormalization(profile, kLocale, 0, &delegate);

  // Let the timeout execute.
  scoped_task_environment_.RunUntilIdle();

  // Since the rules are never loaded and the timeout is 0, the delegate should
  // get notified that the address could not be normalized.
  EXPECT_FALSE(delegate.normalized_called());
  EXPECT_TRUE(delegate.not_normalized_called());
}

// Tests that if the rules are not loaded before the call to
// StartAddressNormalization, they will be loaded in the call.
TEST_F(AddressNormalizerTest, StartNormalization_RulesNotLoaded_WillLoad) {
  NormalizationDelegate delegate;
  AutofillProfile profile;

  // Start the normalization.
  normalizer_->StartAddressNormalization(profile, kLocale, 0, &delegate);

  // Even if the rules are not loaded before the call to
  // StartAddressNormalization, they should get loaded in the call. Since our
  // test source is synchronous, the normalization will happen synchronously
  // too.
  EXPECT_TRUE(normalizer_->AreRulesLoadedForRegion(kLocale));
  EXPECT_TRUE(delegate.normalized_called());
  EXPECT_FALSE(delegate.not_normalized_called());
}

// Tests that the phone number is formatted when the address is normalized.
TEST_F(AddressNormalizerTest, FormatPhone_AddressNormalized) {
  NormalizationDelegate delegate;
  AutofillProfile profile;
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16("(515) 123-1234"));

  // Load the rules.
  normalizer_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(normalizer_->AreRulesLoadedForRegion(kLocale));

  // Start the normalization.
  normalizer_->StartAddressNormalization(profile, kLocale, 0, &delegate);

  // Make sure the address was normalized.
  EXPECT_TRUE(delegate.normalized_called());

  // Expect that the phone number was formatted.
  EXPECT_EQ("+15151231234", base::UTF16ToUTF8(delegate.profile().GetRawInfo(
                                PHONE_HOME_WHOLE_NUMBER)));
}

// Tests that the phone number is formatted even when the address is not
// normalized.
TEST_F(AddressNormalizerTest, FormatPhone_AddressNotNormalized) {
  NormalizationDelegate delegate;
  AutofillProfile profile;
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16("515-123-1234"));

  // Make sure the rules will not be loaded in the StartAddressNormalization
  // call.
  normalizer_->ShouldLoadRules(false);

  // Start the normalization.
  normalizer_->StartAddressNormalization(profile, kLocale, 0, &delegate);

  // Let the timeout execute.
  scoped_task_environment_.RunUntilIdle();

  // Make sure the address was not normalized.
  EXPECT_TRUE(delegate.not_normalized_called());

  // Expect that the phone number was formatted.
  EXPECT_EQ("+15151231234", base::UTF16ToUTF8(delegate.profile().GetRawInfo(
                                PHONE_HOME_WHOLE_NUMBER)));
}

}  // namespace autofill
