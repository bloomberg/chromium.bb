// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_JS_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_JS_JS_TEST_UTIL_H_
#pragma once

#include <ostream>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/js/js_backend.h"
#include "chrome/browser/sync/js/js_controller.h"
#include "chrome/browser/sync/js/js_event_handler.h"
#include "chrome/browser/sync/js/js_reply_handler.h"
#include "chrome/browser/sync/util/weak_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace browser_sync {

class JsArgList;
class JsEventDetails;

// Defined for googletest.  Equivalent to "*os << args.ToString()".
void PrintTo(const JsArgList& args, ::std::ostream* os);
void PrintTo(const JsEventDetails& details, ::std::ostream* os);

// A gmock matcher for JsArgList.  Use like:
//
//   EXPECT_CALL(mock, HandleJsReply("foo", HasArgs(expected_args)));
::testing::Matcher<const JsArgList&> HasArgs(const JsArgList& expected_args);

// Like HasArgs() but takes a ListValue instead.
::testing::Matcher<const JsArgList&> HasArgsAsList(
    const base::ListValue& expected_args);

// A gmock matcher for JsEventDetails.  Use like:
//
//   EXPECT_CALL(mock, HandleJsEvent("foo", HasArgs(expected_details)));
::testing::Matcher<const JsEventDetails&> HasDetails(
    const JsEventDetails& expected_details);

// Like HasDetails() but takes a DictionaryValue instead.
::testing::Matcher<const JsEventDetails&> HasDetailsAsDictionary(
    const base::DictionaryValue& expected_details);

// Mocks.

class MockJsBackend : public JsBackend,
                      public base::SupportsWeakPtr<MockJsBackend> {
 public:
  MockJsBackend();
  virtual ~MockJsBackend();

  WeakHandle<JsBackend> AsWeakHandle();

  MOCK_METHOD1(SetJsEventHandler, void(const WeakHandle<JsEventHandler>&));
  MOCK_METHOD3(ProcessJsMessage, void(const ::std::string&, const JsArgList&,
                                    const WeakHandle<JsReplyHandler>&));
};

class MockJsController : public JsController,
                         public base::SupportsWeakPtr<MockJsController> {
 public:
  MockJsController();
  virtual ~MockJsController();

  MOCK_METHOD1(AddJsEventHandler, void(JsEventHandler*));
  MOCK_METHOD1(RemoveJsEventHandler, void(JsEventHandler*));
  MOCK_METHOD3(ProcessJsMessage,
               void(const ::std::string&, const JsArgList&,
                    const WeakHandle<JsReplyHandler>&));
};

class MockJsEventHandler
    : public JsEventHandler,
      public base::SupportsWeakPtr<MockJsEventHandler> {
 public:
  MockJsEventHandler();
  virtual ~MockJsEventHandler();

  WeakHandle<JsEventHandler> AsWeakHandle();

  MOCK_METHOD2(HandleJsEvent,
               void(const ::std::string&, const JsEventDetails&));
};

class MockJsReplyHandler
    : public JsReplyHandler,
      public base::SupportsWeakPtr<MockJsReplyHandler> {
 public:
  MockJsReplyHandler();
  virtual ~MockJsReplyHandler();

  WeakHandle<JsReplyHandler> AsWeakHandle();

  MOCK_METHOD2(HandleJsReply,
               void(const ::std::string&, const JsArgList&));
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_JS_TEST_UTIL_H_
