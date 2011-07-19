// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_event_handler_list.h"

#include "base/values.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_event_details.h"
#include "chrome/browser/sync/js_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class JsEventHandlerListTest : public testing::Test {};

TEST_F(JsEventHandlerListTest, Basic) {
  InSequence dummy;
  // |backend| must outlive |event_handlers|.
  StrictMock<MockJsBackend> backend;

  JsEventHandlerList event_handlers;

  ListValue arg_list1, arg_list2;
  arg_list1.Append(Value::CreateBooleanValue(false));
  arg_list2.Append(Value::CreateIntegerValue(5));
  JsArgList args1(&arg_list1), args2(&arg_list2);

  DictionaryValue details_dict1, details_dict2;
  details_dict1.SetString("foo", "bar");
  details_dict2.SetInteger("baz", 5);
  JsEventDetails details1(&details_dict1), details2(&details_dict2);

  StrictMock<MockJsEventHandler> handler1, handler2;

  // Once from each call to AddHandler().
  EXPECT_CALL(backend, SetParentJsEventRouter(&event_handlers)).Times(2);

  EXPECT_CALL(backend, ProcessMessage("test1", HasArgs(args2), &handler1));

  EXPECT_CALL(handler2, HandleJsMessageReply("reply2", HasArgs(args1)));
  EXPECT_CALL(handler1, HandleJsMessageReply("reply1", HasArgs(args2)));
  EXPECT_CALL(handler1, HandleJsEvent("event", HasDetails(details1)));
  EXPECT_CALL(handler2, HandleJsEvent("event", HasDetails(details1)));

  EXPECT_CALL(backend, ProcessMessage("test2", HasArgs(args1), &handler2));

  EXPECT_CALL(handler2, HandleJsMessageReply("anotherreply2", HasArgs(args2)));
  EXPECT_CALL(handler2, HandleJsEvent("anotherevent", HasDetails(details2)));

  // Once from the second call to RemoveHandler(), once from the
  // destructor.
  EXPECT_CALL(backend, RemoveParentJsEventRouter()).Times(2);

  event_handlers.SetBackend(&backend);

  event_handlers.AddHandler(&handler1);
  event_handlers.AddHandler(&handler2);

  event_handlers.ProcessMessage("test1", args2, &handler1);

  event_handlers.RouteJsMessageReply("reply2", args1, &handler2);
  event_handlers.RouteJsMessageReply("reply1", args2, &handler1);
  event_handlers.RouteJsEvent("event", details1);

  event_handlers.RemoveHandler(&handler1);

  event_handlers.ProcessMessage("test2", args1, &handler2);

  event_handlers.RouteJsMessageReply("droppedreply1", args1, &handler1);
  event_handlers.RouteJsMessageReply("anotherreply2", args2, &handler2);
  event_handlers.RouteJsEvent("anotherevent", details2);

  event_handlers.RemoveHandler(&handler2);

  event_handlers.RouteJsMessageReply("anotherdroppedreply1",
                                     args1, &handler1);
  event_handlers.RouteJsMessageReply("anotheranotherreply2",
                                     args2, &handler2);
  event_handlers.RouteJsEvent("droppedevent", details2);

  // Let destructor of |event_handlers| call RemoveBackend().
}

TEST_F(JsEventHandlerListTest, QueuedMessages) {
  // |backend| must outlive |event_handlers|.
  StrictMock<MockJsBackend> backend;

  JsEventHandlerList event_handlers;

  ListValue arg_list1, arg_list2;
  arg_list1.Append(Value::CreateBooleanValue(false));
  arg_list2.Append(Value::CreateIntegerValue(5));
  JsArgList args1(&arg_list1), args2(&arg_list2);

  StrictMock<MockJsEventHandler> handler1, handler2;

  // Once from the call to SetBackend().
  EXPECT_CALL(backend, SetParentJsEventRouter(&event_handlers));
  // Once from the first call to RemoveBackend().
  EXPECT_CALL(backend, RemoveParentJsEventRouter());
  // Once from the call to SetBackend().
  EXPECT_CALL(backend, ProcessMessage("test1", HasArgs(args2), &handler1));
  // Once from the call to SetBackend().
  EXPECT_CALL(backend, ProcessMessage("test2", HasArgs(args1), &handler2));

  event_handlers.AddHandler(&handler1);
  event_handlers.AddHandler(&handler2);

  // Should queue messages.
  event_handlers.ProcessMessage("test1", args2, &handler1);
  event_handlers.ProcessMessage("test2", args1, &handler2);

  // Should call the queued messages.
  event_handlers.SetBackend(&backend);

  event_handlers.RemoveBackend();
  // Should do nothing.
  event_handlers.RemoveBackend();
}

}  // namespace
}  // namespace browser_sync
