// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/model_type_test_util.h"

namespace syncable {

void PrintTo(ModelEnumSet model_types, ::std::ostream* os) {
  *os << ModelEnumSetToString(model_types);
}

namespace {

// Matcher implementation for HasModelTypes().
class HasModelTypesMatcher
    : public ::testing::MatcherInterface<ModelEnumSet> {
 public:
  explicit HasModelTypesMatcher(ModelEnumSet expected_types)
      : expected_types_(expected_types) {}

  virtual ~HasModelTypesMatcher() {}

  virtual bool MatchAndExplain(
      ModelEnumSet model_types,
      ::testing::MatchResultListener* listener) const {
    // No need to annotate listener since we already define PrintTo().
    return model_types.Equals(expected_types_);
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "has model types " << ModelEnumSetToString(expected_types_);
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't have model types "
        << ModelEnumSetToString(expected_types_);
  }

 private:
  const ModelEnumSet expected_types_;

  DISALLOW_COPY_AND_ASSIGN(HasModelTypesMatcher);
};

}  // namespace

::testing::Matcher<ModelEnumSet> HasModelTypes(ModelEnumSet expected_types) {
  return ::testing::MakeMatcher(new HasModelTypesMatcher(expected_types));
}

}  // namespace syncable
