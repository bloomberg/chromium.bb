// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/event_matcher.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace extensions {

class EventFilterUnittest : public testing::Test {
 public:
  EventFilterUnittest() {
    google_event_.url = GURL("http://google.com");
    yahoo_event_.url = GURL("http://yahoo.com");
    random_url_event_.url = GURL("http://www.something-else.com");
    empty_url_event_.url = GURL();
  }

 protected:
  std::unique_ptr<base::Value> HostSuffixDict(const std::string& host_suffix) {
    auto dict = std::make_unique<DictionaryValue>();
    dict->SetString("hostSuffix", host_suffix);
    return std::move(dict);
  }

  std::unique_ptr<base::ListValue> ValueAsList(
      std::unique_ptr<base::Value> value) {
    auto result = std::make_unique<base::ListValue>();
    result->Append(std::move(value));
    return result;
  }

  std::unique_ptr<EventMatcher> AllURLs() {
    return std::make_unique<EventMatcher>(
        std::make_unique<base::DictionaryValue>(), MSG_ROUTING_NONE);
  }

  std::unique_ptr<EventMatcher> HostSuffixMatcher(
      const std::string& host_suffix) {
    return MatcherFromURLFilterList(ValueAsList(HostSuffixDict(host_suffix)));
  }

  std::unique_ptr<EventMatcher> MatcherFromURLFilterList(
      std::unique_ptr<ListValue> url_filter_list) {
    auto filter_dict = std::make_unique<DictionaryValue>();
    filter_dict->Set("url", std::move(url_filter_list));
    return std::unique_ptr<EventMatcher>(
        new EventMatcher(std::move(filter_dict), MSG_ROUTING_NONE));
  }

  EventFilter event_filter_;
  EventFilteringInfo empty_event_;
  EventFilteringInfo google_event_;
  EventFilteringInfo yahoo_event_;
  EventFilteringInfo random_url_event_;
  EventFilteringInfo empty_url_event_;
};

TEST_F(EventFilterUnittest, NoMatchersMatchIfEmpty) {
  std::set<int> matches = event_filter_.MatchEvent("some-event",
                                                   empty_event_,
                                                   MSG_ROUTING_NONE);
  ASSERT_EQ(0u, matches.size());
}

TEST_F(EventFilterUnittest, AddingEventMatcherDoesntCrash) {
  event_filter_.AddEventMatcher("event1", AllURLs());
}

TEST_F(EventFilterUnittest,
    DontMatchAgainstMatchersForDifferentEvents) {
  event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches = event_filter_.MatchEvent("event2",
                                                   empty_event_,
                                                   MSG_ROUTING_NONE);
  ASSERT_EQ(0u, matches.size());
}

TEST_F(EventFilterUnittest, DoMatchAgainstMatchersForSameEvent) {
  int id = event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches = event_filter_.MatchEvent("event1",
      google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest, DontMatchUnlessMatcherMatches) {
  EventFilteringInfo info;
  info.url = GURL("http://www.yahoo.com");
  event_filter_.AddEventMatcher("event1", HostSuffixMatcher("google.com"));
  std::set<int> matches = event_filter_.MatchEvent(
      "event1", info, MSG_ROUTING_NONE);
  ASSERT_TRUE(matches.empty());
}

TEST_F(EventFilterUnittest, RemovingAnEventMatcherStopsItMatching) {
  int id = event_filter_.AddEventMatcher("event1", AllURLs());
  event_filter_.RemoveEventMatcher(id);
  std::set<int> matches = event_filter_.MatchEvent("event1",
                                                   empty_event_,
                                                   MSG_ROUTING_NONE);
  ASSERT_TRUE(matches.empty());
}

TEST_F(EventFilterUnittest, MultipleEventMatches) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches = event_filter_.MatchEvent("event1",
      google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(2u, matches.size());
  ASSERT_EQ(1u, matches.count(id1));
  ASSERT_EQ(1u, matches.count(id2));
}

TEST_F(EventFilterUnittest, TestURLMatching) {
  EventFilteringInfo info;
  info.url = GURL("http://www.google.com");
  int id = event_filter_.AddEventMatcher("event1",
                                         HostSuffixMatcher("google.com"));
  std::set<int> matches = event_filter_.MatchEvent(
      "event1", info, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest, TestMultipleURLFiltersMatchOnAny) {
  std::unique_ptr<base::ListValue> filters(new base::ListValue());
  filters->Append(HostSuffixDict("google.com"));
  filters->Append(HostSuffixDict("yahoo.com"));

  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(std::move(filters)));
  int id = event_filter_.AddEventMatcher("event1", std::move(matcher));

  {
    std::set<int> matches = event_filter_.MatchEvent("event1",
        google_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id));
  }
  {
    std::set<int> matches = event_filter_.MatchEvent("event1",
        yahoo_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id));
  }
  {
    std::set<int> matches = event_filter_.MatchEvent("event1",
        random_url_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(0u, matches.size());
  }
}

TEST_F(EventFilterUnittest, TestStillMatchesAfterRemoval) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());

  event_filter_.RemoveEventMatcher(id1);
  {
    std::set<int> matches = event_filter_.MatchEvent("event1",
        google_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id2));
  }
}

TEST_F(EventFilterUnittest, TestMatchesOnlyAgainstPatternsForCorrectEvent) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  event_filter_.AddEventMatcher("event2", AllURLs());

  {
    std::set<int> matches = event_filter_.MatchEvent("event1",
        google_event_, MSG_ROUTING_NONE);
    ASSERT_EQ(1u, matches.size());
    ASSERT_EQ(1u, matches.count(id1));
  }
}

TEST_F(EventFilterUnittest, TestGetMatcherCountForEvent) {
  ASSERT_EQ(0, event_filter_.GetMatcherCountForEventForTesting("event1"));
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  ASSERT_EQ(1, event_filter_.GetMatcherCountForEventForTesting("event1"));
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  ASSERT_EQ(2, event_filter_.GetMatcherCountForEventForTesting("event1"));
  event_filter_.RemoveEventMatcher(id1);
  ASSERT_EQ(1, event_filter_.GetMatcherCountForEventForTesting("event1"));
  event_filter_.RemoveEventMatcher(id2);
  ASSERT_EQ(0, event_filter_.GetMatcherCountForEventForTesting("event1"));
}

TEST_F(EventFilterUnittest, RemoveEventMatcherReturnsEventName) {
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  int id3 = event_filter_.AddEventMatcher("event2", AllURLs());

  ASSERT_EQ("event1", event_filter_.RemoveEventMatcher(id1));
  ASSERT_EQ("event1", event_filter_.RemoveEventMatcher(id2));
  ASSERT_EQ("event2", event_filter_.RemoveEventMatcher(id3));
}

TEST_F(EventFilterUnittest, InvalidURLFilterCantBeAdded) {
  std::unique_ptr<base::ListValue> filter_list(new base::ListValue());
  filter_list->Append(
      std::make_unique<base::ListValue>());  // Should be a dict.
  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(std::move(filter_list)));
  int id1 = event_filter_.AddEventMatcher("event1", std::move(matcher));
  EXPECT_TRUE(event_filter_.IsURLMatcherEmptyForTesting());
  ASSERT_EQ(-1, id1);
}

TEST_F(EventFilterUnittest, EmptyListOfURLFiltersMatchesAllURLs) {
  std::unique_ptr<base::ListValue> filter_list(new base::ListValue());
  std::unique_ptr<EventMatcher> matcher(
      MatcherFromURLFilterList(std::unique_ptr<ListValue>(new ListValue)));
  int id = event_filter_.AddEventMatcher("event1", std::move(matcher));
  std::set<int> matches = event_filter_.MatchEvent("event1",
      google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest,
    InternalURLMatcherShouldBeEmptyWhenThereAreNoEventMatchers) {
  ASSERT_TRUE(event_filter_.IsURLMatcherEmptyForTesting());
  int id = event_filter_.AddEventMatcher("event1",
                                         HostSuffixMatcher("google.com"));
  ASSERT_FALSE(event_filter_.IsURLMatcherEmptyForTesting());
  event_filter_.RemoveEventMatcher(id);
  ASSERT_TRUE(event_filter_.IsURLMatcherEmptyForTesting());
}

TEST_F(EventFilterUnittest, EmptyURLsShouldBeMatchedByEmptyURLFilters) {
  int id = event_filter_.AddEventMatcher("event1", AllURLs());
  std::set<int> matches = event_filter_.MatchEvent(
      "event1", empty_url_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest,
    EmptyURLsShouldBeMatchedByEmptyURLFiltersWithAnEmptyItem) {
  std::unique_ptr<EventMatcher> matcher(MatcherFromURLFilterList(
      ValueAsList(std::unique_ptr<Value>(new DictionaryValue()))));
  int id = event_filter_.AddEventMatcher("event1", std::move(matcher));
  std::set<int> matches = event_filter_.MatchEvent(
      "event1", empty_url_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

}  // namespace extensions
