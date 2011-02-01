// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_JS_TEST_UTIL_H_
#pragma once

#include <ostream>
#include <string>

#include "chrome/browser/sync/js_backend.h"
#include "chrome/browser/sync/js_frontend.h"
#include "chrome/browser/sync/js_event_handler.h"
#include "chrome/browser/sync/js_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"

class ListValue;

namespace browser_sync {

class JsArgList;

// Defined for googletest.  Equivalent to "*os << args.ToString()".
void PrintTo(const JsArgList& args, ::std::ostream* os);

// A matcher for gmock.  Use like:
//
//   EXPECT_CALL(mock, ProcessMessage("foo", HasArgs(args))).Times(1);
::testing::Matcher<const JsArgList&> HasArgs(const JsArgList& expected_args);

// Like HasArgs() but takes a ListValue instead.
::testing::Matcher<const JsArgList&> HasArgsAsList(
    const ListValue& expected_args);

// Mocks.

class MockJsBackend : public JsBackend {
 public:
  MOCK_METHOD1(SetParentJsEventRouter, void(JsEventRouter*));
  MOCK_METHOD0(RemoveParentJsEventRouter, void());
  MOCK_CONST_METHOD0(GetParentJsEventRouter, const JsEventRouter*());
  MOCK_METHOD3(ProcessMessage, void(const ::std::string&, const JsArgList&,
                                    const JsEventHandler*));
};

class MockJsFrontend : public JsFrontend {
 public:
  MOCK_METHOD1(AddHandler, void(JsEventHandler*));
  MOCK_METHOD1(RemoveHandler, void(JsEventHandler*));
  MOCK_METHOD3(ProcessMessage,
               void(const ::std::string&, const JsArgList&,
                    const JsEventHandler*));
};

class MockJsEventHandler : public JsEventHandler {
 public:
  MOCK_METHOD2(HandleJsEvent, void(const ::std::string&, const JsArgList&));
};

class MockJsEventRouter : public JsEventRouter {
 public:
  MOCK_METHOD3(RouteJsEvent,
               void(const ::std::string&, const JsArgList&,
                    const JsEventHandler*));
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_TEST_UTIL_H_
