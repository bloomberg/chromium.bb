// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/system_session_analyzer_win.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

const uint16_t kIdSessionStart = 6005U;
const uint16_t kIdSessionEnd = 6006U;
const uint16_t kIdSessionEndUnclean = 6008U;

}  // namespace

// Ensure the fetcher retrieves events.
// Note: this test fails if the host system doesn't have at least 1 prior
// session.
TEST(SystemSessionAnalyzerTest, FetchEvents) {
  SystemSessionAnalyzer analyzer(1U);
  std::vector<SystemSessionAnalyzer::EventInfo> events;
  ASSERT_TRUE(analyzer.FetchEvents(&events));
  EXPECT_EQ(3U, events.size());
}

// Ensure the fetcher's retrieved events conform to our expectations.
// Note: this test fails if the host system doesn't have at least 1 prior
// session.
TEST(SystemSessionAnalyzerTest, ValidateEvents) {
  SystemSessionAnalyzer analyzer(1U);
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

// Stubs FetchEvents.
class StubSystemSessionAnalyzer : public SystemSessionAnalyzer {
 public:
  StubSystemSessionAnalyzer() : SystemSessionAnalyzer(10U) {}

  bool FetchEvents(std::vector<EventInfo>* event_infos) const override {
    DCHECK(event_infos);
    *event_infos = events_;
    return true;
  }

  void AddEvent(const EventInfo& info) { events_.push_back(info); }

 private:
  std::vector<EventInfo> events_;
};

TEST(SystemSessionAnalyzerTest, StandardCase) {
  StubSystemSessionAnalyzer analyzer;

  base::Time time = base::Time::Now();
  analyzer.AddEvent({kIdSessionStart, time});
  analyzer.AddEvent(
      {kIdSessionEndUnclean, time - base::TimeDelta::FromSeconds(10)});
  analyzer.AddEvent({kIdSessionStart, time - base::TimeDelta::FromSeconds(20)});
  analyzer.AddEvent({kIdSessionEnd, time - base::TimeDelta::FromSeconds(22)});
  analyzer.AddEvent({kIdSessionStart, time - base::TimeDelta::FromSeconds(28)});

  EXPECT_EQ(SystemSessionAnalyzer::OUTSIDE_RANGE,
            analyzer.IsSessionUnclean(time - base::TimeDelta::FromSeconds(30)));
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(time - base::TimeDelta::FromSeconds(25)));
  EXPECT_EQ(SystemSessionAnalyzer::UNCLEAN,
            analyzer.IsSessionUnclean(time - base::TimeDelta::FromSeconds(20)));
  EXPECT_EQ(SystemSessionAnalyzer::UNCLEAN,
            analyzer.IsSessionUnclean(time - base::TimeDelta::FromSeconds(15)));
  EXPECT_EQ(SystemSessionAnalyzer::UNCLEAN,
            analyzer.IsSessionUnclean(time - base::TimeDelta::FromSeconds(10)));
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(time - base::TimeDelta::FromSeconds(5)));
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(time + base::TimeDelta::FromSeconds(5)));
}

TEST(SystemSessionAnalyzerTest, NoEvent) {
  StubSystemSessionAnalyzer analyzer;
  EXPECT_EQ(SystemSessionAnalyzer::FAILED,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

TEST(SystemSessionAnalyzerTest, TimeInversion) {
  StubSystemSessionAnalyzer analyzer;

  base::Time time = base::Time::Now();
  analyzer.AddEvent({kIdSessionStart, time});
  analyzer.AddEvent({kIdSessionEnd, time + base::TimeDelta::FromSeconds(1)});
  analyzer.AddEvent({kIdSessionStart, time - base::TimeDelta::FromSeconds(1)});

  EXPECT_EQ(SystemSessionAnalyzer::FAILED,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

TEST(SystemSessionAnalyzerTest, IdInversion) {
  StubSystemSessionAnalyzer analyzer;

  base::Time time = base::Time::Now();
  analyzer.AddEvent({kIdSessionStart, time});
  analyzer.AddEvent({kIdSessionStart, time - base::TimeDelta::FromSeconds(1)});
  analyzer.AddEvent({kIdSessionEnd, time - base::TimeDelta::FromSeconds(2)});

  EXPECT_EQ(SystemSessionAnalyzer::FAILED,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

}  // namespace browser_watcher
