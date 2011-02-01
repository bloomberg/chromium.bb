// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for EventsFunnel.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/events_funnel.h"
#include "ceee/testing/utils/mock_static.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::StrEq;

// Test subclass used to provide access to protected functionality in the
// EventsFunnel class.
class TestEventsFunnel : public EventsFunnel {
 public:
  HRESULT CallSendEvent(const char* event_name, const Value& event_args) {
    return SendEvent(event_name, event_args);
  }

  MOCK_METHOD2(SendEventToBroker, HRESULT(const char*, const char*));
};

TEST(EventsFunnelTest, SendEvent) {
  TestEventsFunnel events_funnel;

  static const char* kEventName = "MADness";
  DictionaryValue event_args;
  event_args.SetInteger("Answer to the Ultimate Question of Life,"
                        "the Universe, and Everything", 42);
  event_args.SetString("AYBABTU", "All your base are belong to us");
  event_args.SetDouble("www.unrealtournament.com", 3.0);

  ListValue args_list;
  args_list.Append(event_args.DeepCopy());

  std::string event_args_str;
  base::JSONWriter::Write(&args_list, false, &event_args_str);

  EXPECT_CALL(events_funnel, SendEventToBroker(StrEq(kEventName),
      StrEq(event_args_str))).Times(1);
  EXPECT_HRESULT_SUCCEEDED(
      events_funnel.CallSendEvent(kEventName, event_args));
}

}  // namespace
