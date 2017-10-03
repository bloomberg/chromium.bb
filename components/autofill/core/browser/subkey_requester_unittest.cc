// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/subkey_requester.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
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

const char kLocale[] = "OZ";
const char kLanguage[] = "en";
const int kInvalidSize = -1;
const int kCorrectSize = 2;  // for subkeys = Do, Re
const int kEmptySize = 0;

class SubKeyReceiver : public base::RefCountedThreadSafe<SubKeyReceiver> {
 public:
  SubKeyReceiver() : subkeys_size_(kInvalidSize) {}

  void OnSubKeysReceived(const std::vector<std::string>& subkeys_codes,
                         const std::vector<std::string>& subkeys_names) {
    subkeys_size_ = subkeys_codes.size();
  }

  int subkeys_size() const { return subkeys_size_; }

 private:
  friend class base::RefCountedThreadSafe<SubKeyReceiver>;
  ~SubKeyReceiver() {}

  int subkeys_size_;

  DISALLOW_COPY_AND_ASSIGN(SubKeyReceiver);
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
        new std::string(
            "{\"data/OZ\": "
            "{\"id\":\"data/OZ\",\"key\":\"OZ\",\"name\":\"Oz \", "
            "\"lang\":\"en\",\"sub_keys\":\"DO~RE\", \"sub_names\":\"Do~Re\"},"
            "\"data/OZ/DO\": "
            "{\"id\":\"data/OZ/DO\",\"key\":\"DO\",\"name\":\"Do \", "
            "\"lang\":\"en\"},"
            "\"data/OZ/RE\": "
            "{\"id\":\"data/OZ/RE\",\"key\":\"RE\",\"name\":\"Re \", "
            "\"lang\":\"en\"}}"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumTestdataSource);
};

// A test subclass of the SubKeyRequesterImpl. Used to simulate rules not
// being loaded.
class TestSubKeyRequester : public SubKeyRequester {
 public:
  TestSubKeyRequester(std::unique_ptr<::i18n::addressinput::Source> source,
                      std::unique_ptr<::i18n::addressinput::Storage> storage)
      : SubKeyRequester(std::move(source), std::move(storage)),
        should_load_rules_(true) {}

  ~TestSubKeyRequester() override {}

  void ShouldLoadRules(bool should_load_rules) {
    should_load_rules_ = should_load_rules;
  }

  void LoadRulesForRegion(const std::string& region_code) override {
    if (should_load_rules_) {
      SubKeyRequester::LoadRulesForRegion(region_code);
    }
  }

 private:
  bool should_load_rules_;

  DISALLOW_COPY_AND_ASSIGN(TestSubKeyRequester);
};

}  // namespace

class SubKeyRequesterTest : public testing::Test {
 protected:
  SubKeyRequesterTest()
      : requester_(new TestSubKeyRequester(
            std::unique_ptr<Source>(new ChromiumTestdataSource),
            std::unique_ptr<Storage>(new NullStorage))) {}

  ~SubKeyRequesterTest() override {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  const std::unique_ptr<TestSubKeyRequester> requester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SubKeyRequesterTest);
};

// Tests that rules are not loaded by default.
TEST_F(SubKeyRequesterTest, AreRulesLoadedForRegion_NotLoaded) {
  EXPECT_FALSE(requester_->AreRulesLoadedForRegion(kLocale));
}

// Tests that the rules are loaded correctly.
TEST_F(SubKeyRequesterTest, AreRulesLoadedForRegion_Loaded) {
  requester_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(requester_->AreRulesLoadedForRegion(kLocale));
}

// Tests that if the rules are loaded before the subkey request is started, the
// received subkeys will be returned to the delegate synchronously.
TEST_F(SubKeyRequesterTest, StartRequest_RulesLoaded) {
  scoped_refptr<SubKeyReceiver> subkey_receiver_ = new SubKeyReceiver();

  SubKeyReceiverCallback cb =
      base::BindOnce(&SubKeyReceiver::OnSubKeysReceived, subkey_receiver_);

  // Load the rules.
  requester_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(requester_->AreRulesLoadedForRegion(kLocale));

  // Start the request.
  requester_->StartRegionSubKeysRequest(kLocale, kLanguage, 0, std::move(cb));

  // Since the rules are already loaded, the subkeys should be received
  // synchronously.
  EXPECT_EQ(subkey_receiver_->subkeys_size(), kCorrectSize);
}

// Tests that if the rules are not loaded before the request and cannot be
// loaded after, the subkeys will not be received and the delegate will be
// notified.
TEST_F(SubKeyRequesterTest, StartRequest_RulesNotLoaded_WillNotLoad) {
  scoped_refptr<SubKeyReceiver> subkey_receiver_ = new SubKeyReceiver();

  SubKeyReceiverCallback cb =
      base::BindOnce(&SubKeyReceiver::OnSubKeysReceived, subkey_receiver_);

  // Make sure the rules will not be loaded in the StartRegionSubKeysRequest
  // call.
  requester_->ShouldLoadRules(false);

  // Start the normalization.
  requester_->StartRegionSubKeysRequest(kLocale, kLanguage, 0, std::move(cb));

  // Let the timeout execute.
  scoped_task_environment_.RunUntilIdle();

  // Since the rules are never loaded and the timeout is 0, the delegate should
  // get notified that the subkeys could not be received.
  EXPECT_EQ(subkey_receiver_->subkeys_size(), kEmptySize);
}

// Tests that if the rules are not loaded before the call to
// StartRegionSubKeysRequest, they will be loaded in the call.
TEST_F(SubKeyRequesterTest, StartRequest_RulesNotLoaded_WillLoad) {
  scoped_refptr<SubKeyReceiver> subkey_receiver_ = new SubKeyReceiver();

  SubKeyReceiverCallback cb =
      base::BindOnce(&SubKeyReceiver::OnSubKeysReceived, subkey_receiver_);

  // Make sure the rules will not be loaded in the StartRegionSubKeysRequest
  // call.
  requester_->ShouldLoadRules(true);
  // Start the request.
  requester_->StartRegionSubKeysRequest(kLocale, kLanguage, 0, std::move(cb));

  // Even if the rules are not loaded before the call to
  // StartRegionSubKeysRequest, they should get loaded in the call. Since our
  // test source is synchronous, the request will happen synchronously
  // too.
  EXPECT_TRUE(requester_->AreRulesLoadedForRegion(kLocale));
  EXPECT_EQ(subkey_receiver_->subkeys_size(), kCorrectSize);
}

}  // namespace autofill
