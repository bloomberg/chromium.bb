// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_event_handler_list.h"

#include "base/values.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::StrictMock;

class JsEventHandlerListTest : public testing::Test {};

TEST_F(JsEventHandlerListTest, Basic) {
  // |backend| must outlive |event_handlers|.
  StrictMock<MockJsBackend> backend;

  JsEventHandlerList event_handlers;

  ListValue arg_list1, arg_list2;
  arg_list1.Append(Value::CreateBooleanValue(false));
  arg_list2.Append(Value::CreateIntegerValue(5));
  JsArgList args1(arg_list1), args2(arg_list2);

  StrictMock<MockJsEventHandler> handler1, handler2;

  // Once from each call to AddHandler().
  EXPECT_CALL(backend, SetParentJsEventRouter(&event_handlers)).Times(2);
  // Once from the second RemoveHandler(), once from the destructor.
  EXPECT_CALL(backend, RemoveParentJsEventRouter()).Times(2);
  EXPECT_CALL(backend, ProcessMessage("test1", HasArgs(args2), &handler1));
  EXPECT_CALL(backend, ProcessMessage("test2", HasArgs(args1), &handler2));

  EXPECT_CALL(handler1, HandleJsEvent("reply1", HasArgs(args2)));
  EXPECT_CALL(handler1, HandleJsEvent("allreply", HasArgs(args1)));

  EXPECT_CALL(handler2, HandleJsEvent("reply2", HasArgs(args1)));
  EXPECT_CALL(handler2, HandleJsEvent("allreply", HasArgs(args1)));
  EXPECT_CALL(handler2, HandleJsEvent("anotherreply2", HasArgs(args2)));
  EXPECT_CALL(handler2, HandleJsEvent("anotherallreply", HasArgs(args2)));

  event_handlers.SetBackend(&backend);

  event_handlers.AddHandler(&handler1);
  event_handlers.AddHandler(&handler2);

  event_handlers.ProcessMessage("test1", args2, &handler1);

  event_handlers.RouteJsEvent("reply2", args1, &handler2);
  event_handlers.RouteJsEvent("reply1", args2, &handler1);
  event_handlers.RouteJsEvent("allreply", args1, NULL);

  event_handlers.RemoveHandler(&handler1);

  event_handlers.ProcessMessage("test2", args1, &handler2);

  event_handlers.RouteJsEvent("droppedreply1", args1, &handler1);
  event_handlers.RouteJsEvent("anotherreply2", args2, &handler2);
  event_handlers.RouteJsEvent("anotherallreply", args2, NULL);

  event_handlers.RemoveHandler(&handler2);

  event_handlers.RouteJsEvent("anotherdroppedreply1", args1, &handler1);
  event_handlers.RouteJsEvent("anotheranotherreply2", args2, &handler2);
  event_handlers.RouteJsEvent("droppedallreply", args2, NULL);

  // Let destructor of |event_handlers| call RemoveBackend().
}

TEST_F(JsEventHandlerListTest, QueuedMessages) {
  // |backend| must outlive |event_handlers|.
  StrictMock<MockJsBackend> backend;

  JsEventHandlerList event_handlers;

  ListValue arg_list1, arg_list2;
  arg_list1.Append(Value::CreateBooleanValue(false));
  arg_list2.Append(Value::CreateIntegerValue(5));
  JsArgList args1(arg_list1), args2(arg_list2);

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
