// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for InfobarEventsFunnel.

#include "ceee/ie/plugin/bho/infobar_events_funnel.h"
#include "base/values.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::Return;
using testing::StrEq;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

const char kOnDocumentCompleteEventName[] = "infobar.onDocumentComplete";

class TestInfobarEventsFunnel : public InfobarEventsFunnel {
 public:
  MOCK_METHOD2(SendEvent, HRESULT(const char*, const Value&));
};

TEST(InfobarEventsFunnelTest, OnDocumentComplete) {
  TestInfobarEventsFunnel infobar_events_funnel;
  DictionaryValue dict;

  EXPECT_CALL(infobar_events_funnel, SendEvent(
    StrEq(kOnDocumentCompleteEventName), ValuesEqual(&dict))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(infobar_events_funnel.OnDocumentComplete());
}

}  // namespace
