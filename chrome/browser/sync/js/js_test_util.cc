// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js/js_test_util.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_event_details.h"

namespace browser_sync {

void PrintTo(const JsArgList& args, ::std::ostream* os) {
  *os << args.ToString();
}

void PrintTo(const JsEventDetails& details, ::std::ostream* os) {
  *os << details.ToString();
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

// Matcher implementation for HasDetails().
class HasDetailsMatcher
    : public ::testing::MatcherInterface<const JsEventDetails&> {
 public:
  explicit HasDetailsMatcher(const JsEventDetails& expected_details)
      : expected_details_(expected_details) {}

  virtual ~HasDetailsMatcher() {}

  virtual bool MatchAndExplain(
      const JsEventDetails& details,
      ::testing::MatchResultListener* listener) const {
    // No need to annotate listener since we already define PrintTo().
    return details.Get().Equals(&expected_details_.Get());
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "has details " << expected_details_.ToString();
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't have details " << expected_details_.ToString();
  }

 private:
  const JsEventDetails expected_details_;

  DISALLOW_COPY_AND_ASSIGN(HasDetailsMatcher);
};

}  // namespace

::testing::Matcher<const JsArgList&> HasArgs(const JsArgList& expected_args) {
  return ::testing::MakeMatcher(new HasArgsMatcher(expected_args));
}

::testing::Matcher<const JsArgList&> HasArgsAsList(
    const ListValue& expected_args) {
  scoped_ptr<ListValue> expected_args_copy(expected_args.DeepCopy());
  return HasArgs(JsArgList(expected_args_copy.get()));
}

::testing::Matcher<const JsEventDetails&> HasDetails(
    const JsEventDetails& expected_details) {
  return ::testing::MakeMatcher(new HasDetailsMatcher(expected_details));
}

::testing::Matcher<const JsEventDetails&> HasDetailsAsDictionary(
    const DictionaryValue& expected_details) {
  scoped_ptr<DictionaryValue> expected_details_copy(
      expected_details.DeepCopy());
  return HasDetails(JsEventDetails(expected_details_copy.get()));
}

MockJsBackend::MockJsBackend() {}

MockJsBackend::~MockJsBackend() {}

WeakHandle<JsBackend> MockJsBackend::AsWeakHandle() {
  return MakeWeakHandle(AsWeakPtr());
}

MockJsController::MockJsController() {}

MockJsController::~MockJsController() {}

MockJsEventHandler::MockJsEventHandler() {}

WeakHandle<JsEventHandler> MockJsEventHandler::AsWeakHandle() {
  return MakeWeakHandle(AsWeakPtr());
}

MockJsEventHandler::~MockJsEventHandler() {}

MockJsReplyHandler::MockJsReplyHandler() {}

MockJsReplyHandler::~MockJsReplyHandler() {}

WeakHandle<JsReplyHandler> MockJsReplyHandler::AsWeakHandle() {
  return MakeWeakHandle(AsWeakPtr());
}

}  // namespace browser_sync

