// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_field_trial.h"

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests ContextualSearchFieldTrail class.
class ContextualSearchFieldTrailTest : public testing::Test {
 public:
  ContextualSearchFieldTrailTest() {}
  ~ContextualSearchFieldTrailTest() override {}

  // Inner class that stubs out access to Variations and command-line switches.
  class ContextualSearchFieldTrailStubbed : public ContextualSearchFieldTrial {
   public:
    // Use these to set a non-empty value to override return of a Get method.
    void SetSwitchValue(const std::string& value);
    void SetParamValue(const std::string& value);

   protected:
    // These are overridden to return the Set value above.
    bool HasSwitch(const std::string& name) override;
    std::string GetSwitch(const std::string& name) override;
    std::string GetParam(const std::string& name) override;

   private:
    bool does_have_switch_;
    std::string switch_value_;
    std::string param_value_;
  };

  // The class under test.
  std::unique_ptr<ContextualSearchFieldTrailStubbed> field_trial_;

 protected:
  void SetUp() override {
    field_trial_.reset(new ContextualSearchFieldTrailStubbed());
  }

  void TearDown() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextualSearchFieldTrailTest);
};

bool ContextualSearchFieldTrailTest::ContextualSearchFieldTrailStubbed::
    HasSwitch(const std::string& name) {
  return does_have_switch_;
}

void ContextualSearchFieldTrailTest::ContextualSearchFieldTrailStubbed::
    SetSwitchValue(const std::string& value) {
  switch_value_ = value;
  does_have_switch_ = true;
}

std::string
ContextualSearchFieldTrailTest::ContextualSearchFieldTrailStubbed::GetSwitch(
    const std::string& name) {
  return switch_value_;
}

void ContextualSearchFieldTrailTest::ContextualSearchFieldTrailStubbed::
    SetParamValue(const std::string& value) {
  param_value_ = value;
}

std::string
ContextualSearchFieldTrailTest::ContextualSearchFieldTrailStubbed::GetParam(
    const std::string& name) {
  return param_value_;
}

TEST_F(ContextualSearchFieldTrailTest, IntegerDefaultValue) {
  // Should return this default value.
  EXPECT_EQ(
      ContextualSearchFieldTrial::kContextualSearchDefaultIcingSurroundingSize,
      field_trial_->GetIcingSurroundingSize());
}

TEST_F(ContextualSearchFieldTrailTest, IntegerParamOverrides) {
  // Params override defaults.
  field_trial_->SetParamValue("500");
  EXPECT_EQ(500, field_trial_->GetIcingSurroundingSize());
}

TEST_F(ContextualSearchFieldTrailTest, IntegerSwitchOverrides) {
  field_trial_->SetParamValue("500");
  // Switches override params.
  field_trial_->SetSwitchValue("200");
  EXPECT_EQ(200, field_trial_->GetIcingSurroundingSize());
}

TEST_F(ContextualSearchFieldTrailTest, IntegerJunkIgnored) {
  // A junk value effectively resets the switch.
  field_trial_->SetSwitchValue("foo");
  EXPECT_EQ(
      ContextualSearchFieldTrial::kContextualSearchDefaultIcingSurroundingSize,
      field_trial_->GetIcingSurroundingSize());
}

TEST_F(ContextualSearchFieldTrailTest, BooleanDefaultValue) {
  // Should return this default value.
  EXPECT_FALSE(field_trial_->IsSendBasePageURLDisabled());
}

TEST_F(ContextualSearchFieldTrailTest, BooleanParamOverrides) {
  // Params override defaults.
  field_trial_->SetParamValue("any");
  EXPECT_TRUE(field_trial_->IsSendBasePageURLDisabled());
}

TEST_F(ContextualSearchFieldTrailTest, BooleanFalseParam) {
  field_trial_->SetParamValue("false");
  EXPECT_FALSE(field_trial_->IsSendBasePageURLDisabled());
}

TEST_F(ContextualSearchFieldTrailTest, BooleanSwitchOverrides) {
  field_trial_->SetParamValue("false");
  // Switches override params.
  field_trial_->SetSwitchValue("any");
  EXPECT_TRUE(field_trial_->IsSendBasePageURLDisabled());
}

TEST_F(ContextualSearchFieldTrailTest, BooleanEmptySwitch) {
  // An empty switch that's present should return true;
  field_trial_->SetSwitchValue("");
  EXPECT_TRUE(field_trial_->IsSendBasePageURLDisabled());
}

TEST_F(ContextualSearchFieldTrailTest, StringDefaultEmpty) {
  // Default should return an empty string.
  EXPECT_TRUE(field_trial_->GetResolverURLPrefix().empty());
}

TEST_F(ContextualSearchFieldTrailTest, StringParamOverrides) {
  // Params override.
  field_trial_->SetParamValue("param");
  EXPECT_EQ("param", field_trial_->GetResolverURLPrefix());
}

TEST_F(ContextualSearchFieldTrailTest, StringSwitchOverrides) {
  field_trial_->SetParamValue("param");
  // Switches override params.
  field_trial_->SetSwitchValue("switch");
  EXPECT_EQ("switch", field_trial_->GetResolverURLPrefix());
}
