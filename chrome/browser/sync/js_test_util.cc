// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_test_util.h"

#include "base/basictypes.h"
#include "chrome/browser/sync/js_arg_list.h"

namespace browser_sync {

void PrintTo(const JsArgList& args, ::std::ostream* os) {
  *os << args.ToString();
}

namespace {

// Matcher implementation for HasArgs().
class HasArgsMatcher
    : public ::testing::MatcherInterface<const JsArgList&> {
 public:
  explicit HasArgsMatcher(const JsArgList& expected_args)
      : expected_args_(expected_args) {}

  virtual ~HasArgsMatcher() {}

  virtual bool MatchAndExplain(
      const JsArgList& args,
      ::testing::MatchResultListener* listener) const {
    // No need to annotate listener since we already define PrintTo().
    return args.Get().Equals(&expected_args_.Get());
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "has args " << expected_args_.ToString();
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't have args " << expected_args_.ToString();
  }

 private:
  const JsArgList expected_args_;

  DISALLOW_COPY_AND_ASSIGN(HasArgsMatcher);
};

}  // namespace

::testing::Matcher<const JsArgList&> HasArgs(const JsArgList& expected_args) {
  return ::testing::MakeMatcher(new HasArgsMatcher(expected_args));
}

::testing::Matcher<const JsArgList&> HasArgsAsList(
    const ListValue& expected_args) {
  return HasArgs(JsArgList(expected_args));
}

}  // namespace browser_sync

