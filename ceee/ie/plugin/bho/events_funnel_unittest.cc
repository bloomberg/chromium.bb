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

MOCK_STATIC_CLASS_BEGIN(MockCeeeModuleUtils)
  MOCK_STATIC_INIT_BEGIN(MockCeeeModuleUtils)
    MOCK_STATIC_INIT2(ceee_module_util::FireEventToBroker,
                      FireEventToBroker);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC2(void, , FireEventToBroker, const std::string&,
                                          const std::string&);
MOCK_STATIC_CLASS_END(MockCeeeModuleUtils)

// Test subclass used to provide access to protected functionality in the
// EventsFunnel class.
class TestEventsFunnel : public EventsFunnel {
 public:
  TestEventsFunnel() : EventsFunnel(true) {}

  HRESULT CallSendEvent(const char* event_name, const Value& event_args) {
    return SendEvent(event_name, event_args);
  }
};

TEST(EventsFunnelTest, SendEvent) {
  TestEventsFunnel events_funnel;
  MockCeeeModuleUtils mock_ceee_module;

  static const char* kEventName = "MADness";
  DictionaryValue event_args;
  event_args.SetInteger("Answer to the Ultimate Question of Life,"
                        "the Universe, and Everything", 42);
  event_args.SetString("AYBABTU", "All your base are belong to us");
  event_args.SetReal("www.unrealtournament.com", 3.0);

  ListValue args_list;
  args_list.Append(event_args.DeepCopy());

  std::string event_args_str;
  base::JSONWriter::Write(&args_list, false, &event_args_str);

  EXPECT_CALL(mock_ceee_module, FireEventToBroker(StrEq(kEventName),
      StrEq(event_args_str))).Times(1);
  EXPECT_HRESULT_SUCCEEDED(
      events_funnel.CallSendEvent(kEventName, event_args));
}

}  // namespace
