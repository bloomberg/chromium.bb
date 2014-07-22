// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/variations/processed_study.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

// Converts |time| to Study proto format.
int64 TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

// Constants for testing associating command line flags with trial groups.
const char kFlagStudyName[] = "flag_test_trial";
const char kFlagGroup1Name[] = "flag_group1";
const char kFlagGroup2Name[] = "flag_group2";
const char kNonFlagGroupName[] = "non_flag_group";
const char kForcingFlag1[] = "flag_test1";
const char kForcingFlag2[] = "flag_test2";

const VariationID kExperimentId = 123;

// Adds an experiment to |study| with the specified |name| and |probability|.
Study_Experiment* AddExperiment(const std::string& name, int probability,
                                Study* study) {
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

// Populates |study| with test data used for testing associating command line
// flags with trials groups. The study will contain three groups, a default
// group that isn't associated with a flag, and two other groups, both
// associated with different flags.
Study CreateStudyWithFlagGroups(int default_group_probability,
                                int flag_group1_probability,
                                int flag_group2_probability) {
  DCHECK_GE(default_group_probability, 0);
  DCHECK_GE(flag_group1_probability, 0);
  DCHECK_GE(flag_group2_probability, 0);
  Study study;
  study.set_name(kFlagStudyName);
  study.set_default_experiment_name(kNonFlagGroupName);

  AddExperiment(kNonFlagGroupName, default_group_probability, &study);
  AddExperiment(kFlagGroup1Name, flag_group1_probability, &study)
      ->set_forcing_flag(kForcingFlag1);
  AddExperiment(kFlagGroup2Name, flag_group2_probability, &study)
      ->set_forcing_flag(kForcingFlag2);

  return study;
}

// Tests whether a field trial is active (i.e. group() has been called on it).
bool IsFieldTrialActive(const std::string& trial_name) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (size_t i = 0; i < active_groups.size(); ++i) {
    if (active_groups[i].trial_name == trial_name)
      return true;
  }
  return false;
}

class TestOverrideStringCallback {
 public:
  typedef std::map<uint32_t, base::string16> OverrideMap;

  TestOverrideStringCallback()
      : callback_(base::Bind(&TestOverrideStringCallback::Override,
                             base::Unretained(this))) {}

  virtual ~TestOverrideStringCallback() {}

  const VariationsSeedProcessor::UIStringOverrideCallback& callback() const {
    return callback_;
  }

  const OverrideMap& overrides() const { return overrides_; }

 private:
  void Override(uint32_t hash, const base::string16& string) {
    overrides_[hash] = string;
  }

  VariationsSeedProcessor::UIStringOverrideCallback callback_;
  OverrideMap overrides_;

  DISALLOW_COPY_AND_ASSIGN(TestOverrideStringCallback);
};

}  // namespace

class VariationsSeedProcessorTest : public ::testing::Test {
 public:
  VariationsSeedProcessorTest() {
  }

  virtual ~VariationsSeedProcessorTest() {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

  bool CreateTrialFromStudy(const Study* study) {
    ProcessedStudy processed_study;
    if (processed_study.Init(study, false)) {
      VariationsSeedProcessor().CreateTrialFromStudy(
          processed_study, override_callback_.callback());
      return true;
    }
    return false;
  }

 protected:
  TestOverrideStringCallback override_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VariationsSeedProcessorTest);
};

TEST_F(VariationsSeedProcessorTest, AllowForceGroupAndVariationId) {
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  study.mutable_experiment(1)->set_google_web_experiment_id(kExperimentId);

  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));

  VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, kFlagStudyName,
                                        kFlagGroup1Name);
  EXPECT_EQ(kExperimentId, id);
}

// Test that the group for kForcingFlag1 is forced.
TEST_F(VariationsSeedProcessorTest, ForceGroupWithFlag1) {
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

// Test that the group for kForcingFlag2 is forced.
TEST_F(VariationsSeedProcessorTest, ForceGroupWithFlag2) {
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kFlagGroup2Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest, ForceGroup_ChooseFirstGroupWithFlag) {
  // Add the flag to the command line arguments so the flag group is forced.
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag2);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest, ForceGroup_DontChooseGroupWithFlag) {
  base::FieldTrialList field_trial_list(NULL);

  // The two flag groups are given high probability, which would normally make
  // them very likely to be chosen. They won't be chosen since flag groups are
  // never chosen when their flag isn't present.
  Study study = CreateStudyWithFlagGroups(1, 999, 999);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kNonFlagGroupName,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

TEST_F(VariationsSeedProcessorTest,
       NonExpiredStudyPrioritizedOverExpiredStudy) {
  VariationsSeedProcessor seed_processor;

  const std::string kTrialName = "A";
  const std::string kGroup1Name = "Group1";

  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name(kTrialName);
  study1->set_default_experiment_name("Default");
  AddExperiment(kGroup1Name, 100, study1);
  AddExperiment("Default", 0, study1);
  Study* study2 = seed.add_study();
  *study2 = *study1;
  ASSERT_EQ(seed.study(0).name(), seed.study(1).name());

  const base::Time year_ago =
      base::Time::Now() - base::TimeDelta::FromDays(365);

  const base::Version version("20.0.0.0");

  // Check that adding [expired, non-expired] activates the non-expired one.
  ASSERT_EQ(std::string(), base::FieldTrialList::FindFullName(kTrialName));
  {
    base::FieldTrialList field_trial_list(NULL);
    study1->set_expiry_date(TimeToProtoTime(year_ago));
    seed_processor.CreateTrialsFromSeed(seed,
                                        "en-CA",
                                        base::Time::Now(),
                                        version,
                                        Study_Channel_STABLE,
                                        Study_FormFactor_DESKTOP,
                                        "",
                                        override_callback_.callback());
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrialName));
  }

  // Check that adding [non-expired, expired] activates the non-expired one.
  ASSERT_EQ(std::string(), base::FieldTrialList::FindFullName(kTrialName));
  {
    base::FieldTrialList field_trial_list(NULL);
    study1->clear_expiry_date();
    study2->set_expiry_date(TimeToProtoTime(year_ago));
    seed_processor.CreateTrialsFromSeed(seed,
                                        "en-CA",
                                        base::Time::Now(),
                                        version,
                                        Study_Channel_STABLE,
                                        Study_FormFactor_DESKTOP,
                                        "",
                                        override_callback_.callback());
    EXPECT_EQ(kGroup1Name, base::FieldTrialList::FindFullName(kTrialName));
  }
}

TEST_F(VariationsSeedProcessorTest, OverrideUIStrings) {
  base::FieldTrialList field_trial_list(NULL);

  Study study;
  study.set_name("Study1");
  study.set_default_experiment_name("B");
  study.set_activation_type(Study_ActivationType_ACTIVATION_AUTO);

  Study_Experiment* experiment1 = AddExperiment("A", 0, &study);
  Study_Experiment_OverrideUIString* override =
      experiment1->add_override_ui_string();

  override->set_name_hash(1234);
  override->set_value("test");

  Study_Experiment* experiment2 = AddExperiment("B", 1, &study);

  EXPECT_TRUE(CreateTrialFromStudy(&study));

  const TestOverrideStringCallback::OverrideMap& overrides =
      override_callback_.overrides();

  EXPECT_TRUE(overrides.empty());

  study.set_name("Study2");
  experiment1->set_probability_weight(1);
  experiment2->set_probability_weight(0);

  EXPECT_TRUE(CreateTrialFromStudy(&study));

  EXPECT_EQ(1u, overrides.size());
  TestOverrideStringCallback::OverrideMap::const_iterator it =
      overrides.find(1234);
  EXPECT_EQ(base::ASCIIToUTF16("test"), it->second);
}

TEST_F(VariationsSeedProcessorTest, OverrideUIStringsWithForcingFlag) {
  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  ASSERT_EQ(kForcingFlag1, study.experiment(1).forcing_flag());

  study.set_activation_type(Study_ActivationType_ACTIVATION_AUTO);
  Study_Experiment_OverrideUIString* override =
      study.mutable_experiment(1)->add_override_ui_string();
  override->set_name_hash(1234);
  override->set_value("test");

  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  base::FieldTrialList field_trial_list(NULL);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kFlagGroup1Name, base::FieldTrialList::FindFullName(study.name()));

  const TestOverrideStringCallback::OverrideMap& overrides =
      override_callback_.overrides();
  EXPECT_EQ(1u, overrides.size());
  TestOverrideStringCallback::OverrideMap::const_iterator it =
      overrides.find(1234);
  EXPECT_EQ(base::ASCIIToUTF16("test"), it->second);
}

TEST_F(VariationsSeedProcessorTest, ValidateStudy) {
  Study study;
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study_Experiment* default_group = AddExperiment("def", 200, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_EQ(300, processed_study.total_probability());

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.2.3");
  EXPECT_TRUE(processed_study.Init(&study, false));

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("2.3.4");
  EXPECT_TRUE(processed_study.Init(&study, false));

  study.clear_default_experiment_name();
  EXPECT_FALSE(processed_study.Init(&study, false));

  study.set_default_experiment_name("xyz");
  EXPECT_FALSE(processed_study.Init(&study, false));

  study.set_default_experiment_name("def");
  default_group->clear_name();
  EXPECT_FALSE(processed_study.Init(&study, false));

  default_group->set_name("def");
  EXPECT_TRUE(processed_study.Init(&study, false));
  Study_Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  EXPECT_FALSE(processed_study.Init(&study, false));
}

TEST_F(VariationsSeedProcessorTest, VariationParams) {
  base::FieldTrialList field_trial_list(NULL);

  Study study;
  study.set_name("Study1");
  study.set_default_experiment_name("B");

  Study_Experiment* experiment1 = AddExperiment("A", 1, &study);
  Study_Experiment_Param* param = experiment1->add_param();
  param->set_name("x");
  param->set_value("y");

  Study_Experiment* experiment2 = AddExperiment("B", 0, &study);

  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ("y", GetVariationParamValue("Study1", "x"));

  study.set_name("Study2");
  experiment1->set_probability_weight(0);
  experiment2->set_probability_weight(1);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(std::string(), GetVariationParamValue("Study2", "x"));
}

TEST_F(VariationsSeedProcessorTest, VariationParamsWithForcingFlag) {
  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  ASSERT_EQ(kForcingFlag1, study.experiment(1).forcing_flag());
  Study_Experiment_Param* param = study.mutable_experiment(1)->add_param();
  param->set_name("x");
  param->set_value("y");

  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);
  base::FieldTrialList field_trial_list(NULL);
  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_EQ(kFlagGroup1Name, base::FieldTrialList::FindFullName(study.name()));
  EXPECT_EQ("y", GetVariationParamValue(study.name(), "x"));
}

TEST_F(VariationsSeedProcessorTest, StartsActive) {
  base::FieldTrialList field_trial_list(NULL);

  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name("A");
  study1->set_default_experiment_name("Default");
  AddExperiment("AA", 100, study1);
  AddExperiment("Default", 0, study1);

  Study* study2 = seed.add_study();
  study2->set_name("B");
  study2->set_default_experiment_name("Default");
  AddExperiment("BB", 100, study2);
  AddExperiment("Default", 0, study2);
  study2->set_activation_type(Study_ActivationType_ACTIVATION_AUTO);

  Study* study3 = seed.add_study();
  study3->set_name("C");
  study3->set_default_experiment_name("Default");
  AddExperiment("CC", 100, study3);
  AddExperiment("Default", 0, study3);
  study3->set_activation_type(Study_ActivationType_ACTIVATION_EXPLICIT);

  VariationsSeedProcessor seed_processor;
  seed_processor.CreateTrialsFromSeed(seed,
                                      "en-CA",
                                      base::Time::Now(),
                                      base::Version("20.0.0.0"),
                                      Study_Channel_STABLE,
                                      Study_FormFactor_DESKTOP,
                                      "",
                                      override_callback_.callback());

  // Non-specified and ACTIVATION_EXPLICIT should not start active, but
  // ACTIVATION_AUTO should.
  EXPECT_FALSE(IsFieldTrialActive("A"));
  EXPECT_TRUE(IsFieldTrialActive("B"));
  EXPECT_FALSE(IsFieldTrialActive("C"));

  EXPECT_EQ("AA", base::FieldTrialList::FindFullName("A"));
  EXPECT_EQ("BB", base::FieldTrialList::FindFullName("B"));
  EXPECT_EQ("CC", base::FieldTrialList::FindFullName("C"));

  // Now, all studies should be active.
  EXPECT_TRUE(IsFieldTrialActive("A"));
  EXPECT_TRUE(IsFieldTrialActive("B"));
  EXPECT_TRUE(IsFieldTrialActive("C"));
}

TEST_F(VariationsSeedProcessorTest, StartsActiveWithFlag) {
  CommandLine::ForCurrentProcess()->AppendSwitch(kForcingFlag1);

  base::FieldTrialList field_trial_list(NULL);

  Study study = CreateStudyWithFlagGroups(100, 0, 0);
  study.set_activation_type(Study_ActivationType_ACTIVATION_AUTO);

  EXPECT_TRUE(CreateTrialFromStudy(&study));
  EXPECT_TRUE(IsFieldTrialActive(kFlagStudyName));

  EXPECT_EQ(kFlagGroup1Name,
            base::FieldTrialList::FindFullName(kFlagStudyName));
}

}  // namespace chrome_variations
