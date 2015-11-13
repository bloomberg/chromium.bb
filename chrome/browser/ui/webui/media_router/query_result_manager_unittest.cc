// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/ui/webui/media_router/query_result_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ContainerEq;
using testing::Eq;
using testing::IsEmpty;
using testing::Eq;
using testing::Mock;
using testing::Return;
using testing::_;

namespace media_router {

namespace {

class MockObserver : public QueryResultManager::Observer {
 public:
  MOCK_METHOD1(OnResultsUpdated, void(
                   const std::vector<MediaSinkWithCastModes>& sinks));
};

}  // namespace

class QueryResultManagerTest : public ::testing::Test {
 public:
  QueryResultManagerTest()
      : mock_router_(), query_result_manager_(&mock_router_) {
  }

  void DiscoverSinks(MediaCastMode cast_mode, const MediaSource& source) {
    EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_observer_, OnResultsUpdated(_)).Times(1);
    query_result_manager_.StartSinksQuery(cast_mode, source);
  }

  MockMediaRouter mock_router_;
  QueryResultManager query_result_manager_;
  MockObserver mock_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryResultManagerTest);
};

MATCHER_P(VectorEquals, expected, "") {
  if (expected.size() != arg.size()) {
    return false;
  }
  for (int i = 0; i < static_cast<int>(expected.size()); i++) {
    if (!expected[i].Equals(arg[i])) {
      return false;
    }
  }
  return true;
}

TEST_F(QueryResultManagerTest, Observers) {
  MockObserver ob1;
  MockObserver ob2;
  query_result_manager_.AddObserver(&ob1);
  query_result_manager_.AddObserver(&ob2);

  EXPECT_CALL(ob1, OnResultsUpdated(_)).Times(1);
  EXPECT_CALL(ob2, OnResultsUpdated(_)).Times(1);
  query_result_manager_.NotifyOnResultsUpdated();

  query_result_manager_.RemoveObserver(&ob2);
  EXPECT_CALL(ob1, OnResultsUpdated(_)).Times(1);
  query_result_manager_.NotifyOnResultsUpdated();

  query_result_manager_.RemoveObserver(&ob1);
  query_result_manager_.NotifyOnResultsUpdated();
}

TEST_F(QueryResultManagerTest, StartStopSinksQuery) {
  CastModeSet cast_modes;

  query_result_manager_.GetSupportedCastModes(&cast_modes);
  EXPECT_TRUE(cast_modes.empty());
  MediaSource actual_source =
      query_result_manager_.GetSourceForCastMode(MediaCastMode::DEFAULT);
  EXPECT_TRUE(actual_source.Empty());

  MediaSource source(MediaSourceForPresentationUrl("http://fooUrl"));
  EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
      .WillOnce(Return(true));
  query_result_manager_.StartSinksQuery(MediaCastMode::DEFAULT, source);

  query_result_manager_.GetSupportedCastModes(&cast_modes);
  EXPECT_EQ(1u, cast_modes.size());
  EXPECT_TRUE(ContainsKey(cast_modes, MediaCastMode::DEFAULT));
  actual_source = query_result_manager_.GetSourceForCastMode(
      MediaCastMode::DEFAULT);
  EXPECT_TRUE(source.Equals(actual_source));

  // Register a different source for the same cast mode.
  MediaSource another_source(MediaSourceForPresentationUrl("http://barUrl"));
  EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_)).Times(1);
  EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
      .WillOnce(Return(true));
  query_result_manager_.StartSinksQuery(
      MediaCastMode::DEFAULT, another_source);

  query_result_manager_.GetSupportedCastModes(&cast_modes);
  EXPECT_EQ(1u, cast_modes.size());
  EXPECT_TRUE(ContainsKey(cast_modes, MediaCastMode::DEFAULT));
  actual_source = query_result_manager_.GetSourceForCastMode(
      MediaCastMode::DEFAULT);
  EXPECT_TRUE(another_source.Equals(actual_source));

  EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_)).Times(1);
  query_result_manager_.StopSinksQuery(MediaCastMode::DEFAULT);

  query_result_manager_.GetSupportedCastModes(&cast_modes);
  EXPECT_TRUE(cast_modes.empty());
  actual_source = query_result_manager_.GetSourceForCastMode(
      MediaCastMode::DEFAULT);
  EXPECT_TRUE(actual_source.Empty());
}

TEST_F(QueryResultManagerTest, MultipleQueries) {
  MediaSink sink1("sinkId1", "Sink 1", MediaSink::IconType::CAST);
  MediaSink sink2("sinkId2", "Sink 2", MediaSink::IconType::CAST);
  MediaSink sink3("sinkId3", "Sink 3", MediaSink::IconType::CAST);
  MediaSink sink4("sinkId4", "Sink 4", MediaSink::IconType::CAST);
  MediaSink sink5("sinkId5", "Sink 5", MediaSink::IconType::CAST);

  query_result_manager_.AddObserver(&mock_observer_);
  DiscoverSinks(MediaCastMode::DEFAULT,
                MediaSourceForPresentationUrl("http://barUrl"));
  DiscoverSinks(MediaCastMode::TAB_MIRROR, MediaSourceForTab(123));

  // Scenario (results in this order):
  // Action: DEFAULT -> [1, 2, 3]
  // Expected result:
  // Sinks: [1 -> {DEFAULT}, 2 -> {DEFAULT}, 3 -> {DEFAULT}]
  std::vector<MediaSinkWithCastModes> expected_sinks;
  expected_sinks.push_back(MediaSinkWithCastModes(sink1));
  expected_sinks.back().cast_modes.insert(MediaCastMode::DEFAULT);
  expected_sinks.push_back(MediaSinkWithCastModes(sink2));
  expected_sinks.back().cast_modes.insert(MediaCastMode::DEFAULT);
  expected_sinks.push_back(MediaSinkWithCastModes(sink3));
  expected_sinks.back().cast_modes.insert(MediaCastMode::DEFAULT);

  const auto& sinks_observers = query_result_manager_.sinks_observers_;
  auto sinks_observer_it = sinks_observers.find(MediaCastMode::DEFAULT);
  ASSERT_TRUE(sinks_observer_it != sinks_observers.end());
  ASSERT_TRUE(sinks_observer_it->second.get());

  std::vector<MediaSink> sinks_query_result;
  sinks_query_result.push_back(sink1);
  sinks_query_result.push_back(sink2);
  sinks_query_result.push_back(sink3);
  EXPECT_CALL(mock_observer_,
              OnResultsUpdated(VectorEquals(expected_sinks))).Times(1);
  sinks_observer_it->second->OnSinksReceived(sinks_query_result);

  // Action: TAB_MIRROR -> [2, 3, 4]
  // Expected result:
  // Sinks: [1 -> {DEFAULT}, 2 -> {DEFAULT, TAB_MIRROR},
  //         3 -> {DEFAULT, TAB_MIRROR}, 4 -> {TAB_MIRROR}]
  expected_sinks.clear();
  expected_sinks.push_back(MediaSinkWithCastModes(sink1));
  expected_sinks.back().cast_modes.insert(MediaCastMode::DEFAULT);
  expected_sinks.push_back(MediaSinkWithCastModes(sink2));
  expected_sinks.back().cast_modes.insert(MediaCastMode::DEFAULT);
  expected_sinks.back().cast_modes.insert(MediaCastMode::TAB_MIRROR);
  expected_sinks.push_back(MediaSinkWithCastModes(sink3));
  expected_sinks.back().cast_modes.insert(MediaCastMode::DEFAULT);
  expected_sinks.back().cast_modes.insert(MediaCastMode::TAB_MIRROR);
  expected_sinks.push_back(MediaSinkWithCastModes(sink4));
  expected_sinks.back().cast_modes.insert(MediaCastMode::TAB_MIRROR);

  sinks_query_result.clear();
  sinks_query_result.push_back(sink2);
  sinks_query_result.push_back(sink3);
  sinks_query_result.push_back(sink4);

  sinks_observer_it = sinks_observers.find(MediaCastMode::TAB_MIRROR);
  ASSERT_TRUE(sinks_observer_it != sinks_observers.end());
  ASSERT_TRUE(sinks_observer_it->second.get());
  EXPECT_CALL(mock_observer_,
              OnResultsUpdated(VectorEquals(expected_sinks))).Times(1);
  sinks_observer_it->second->OnSinksReceived(sinks_query_result);

  // Action: Update default presentation URL
  // Expected result:
  // Sinks: [2 -> {TAB_MIRROR}, 3 -> {TAB_MIRROR}, 4 -> {TAB_MIRROR}]
  expected_sinks.clear();
  expected_sinks.push_back(MediaSinkWithCastModes(sink2));
  expected_sinks.back().cast_modes.insert(MediaCastMode::TAB_MIRROR);
  expected_sinks.push_back(MediaSinkWithCastModes(sink3));
  expected_sinks.back().cast_modes.insert(MediaCastMode::TAB_MIRROR);
  expected_sinks.push_back(MediaSinkWithCastModes(sink4));
  expected_sinks.back().cast_modes.insert(MediaCastMode::TAB_MIRROR);

  EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_)).Times(1);
  EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_,
              OnResultsUpdated(VectorEquals(expected_sinks))).Times(1);
  query_result_manager_.StartSinksQuery(
      MediaCastMode::DEFAULT,
      MediaSourceForPresentationUrl("http://bazurl.com"));

  // Action: Remove TAB_MIRROR observer
  // Expected result:
  // Sinks: []
  expected_sinks.clear();
  EXPECT_CALL(mock_observer_,
              OnResultsUpdated(VectorEquals(expected_sinks))).Times(1);
  EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_)).Times(1);
  query_result_manager_.StopSinksQuery(MediaCastMode::TAB_MIRROR);

  // Remaining observers: DEFAULT observer, which will be removed on destruction
  EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_)).Times(1);
}

}  // namespace media_router
