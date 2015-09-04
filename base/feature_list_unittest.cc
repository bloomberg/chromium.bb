// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const char kFeatureOnByDefaultName[] = "OnByDefault";
struct Feature kFeatureOnByDefault {
  kFeatureOnByDefaultName, FEATURE_ENABLED_BY_DEFAULT
};

const char kFeatureOffByDefaultName[] = "OffByDefault";
struct Feature kFeatureOffByDefault {
  kFeatureOffByDefaultName, FEATURE_DISABLED_BY_DEFAULT
};

}  // namespace

class FeatureListTest : public testing::Test {
 public:
  FeatureListTest() : feature_list_(nullptr) {
    RegisterFeatureListInstance(make_scoped_ptr(new FeatureList));
  }
  ~FeatureListTest() override { ClearFeatureListInstance(); }

  void RegisterFeatureListInstance(scoped_ptr<FeatureList> feature_list) {
    feature_list_ = feature_list.get();
    FeatureList::SetInstance(feature_list.Pass());
  }
  void ClearFeatureListInstance() {
    FeatureList::ClearInstanceForTesting();
    feature_list_ = nullptr;
  }

  FeatureList* feature_list() { return feature_list_; }

 private:
  // Weak. Owned by the FeatureList::SetInstance().
  FeatureList* feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FeatureListTest);
};

TEST_F(FeatureListTest, DefaultStates) {
  EXPECT_TRUE(FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(FeatureList::IsEnabled(kFeatureOffByDefault));
}

TEST_F(FeatureListTest, InitializeFromCommandLine) {
  struct {
    const char* enable_features;
    const char* disable_features;
    bool expected_feature_on_state;
    bool expected_feature_off_state;
  } test_cases[] = {
      {"", "", true, false},
      {"OffByDefault", "", true, true},
      {"OffByDefault", "OnByDefault", false, true},
      {"OnByDefault,OffByDefault", "", true, true},
      {"", "OnByDefault,OffByDefault", false, false},
      // In the case an entry is both, disable takes precedence.
      {"OnByDefault", "OnByDefault,OffByDefault", false, false},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    const auto& test_case = test_cases[i];

    ClearFeatureListInstance();
    scoped_ptr<FeatureList> feature_list(new FeatureList);
    feature_list->InitializeFromCommandLine(test_case.enable_features,
                                            test_case.disable_features);
    RegisterFeatureListInstance(feature_list.Pass());

    EXPECT_EQ(test_case.expected_feature_on_state,
              FeatureList::IsEnabled(kFeatureOnByDefault))
        << i;
    EXPECT_EQ(test_case.expected_feature_off_state,
              FeatureList::IsEnabled(kFeatureOffByDefault))
        << i;
  }
}

TEST_F(FeatureListTest, CheckFeatureIdentity) {
  // Tests that CheckFeatureIdentity() correctly detects when two different
  // structs with the same feature name are passed to it.

  // Call it twice for each feature at the top of the file, since the first call
  // makes it remember the entry and the second call will verify it.
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOnByDefault));
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOnByDefault));
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOffByDefault));
  EXPECT_TRUE(feature_list()->CheckFeatureIdentity(kFeatureOffByDefault));

  // Now, call it with a distinct struct for |kFeatureOnByDefaultName|, which
  // should return false.
  struct Feature kFeatureOnByDefault2 {
    kFeatureOnByDefaultName, FEATURE_ENABLED_BY_DEFAULT
  };
  EXPECT_FALSE(feature_list()->CheckFeatureIdentity(kFeatureOnByDefault2));
}

}  // namespace base
