// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/common/event_filter.h"
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
    google_event_.SetURL(GURL("http://google.com"));
    yahoo_event_.SetURL(GURL("http://yahoo.com"));
    random_url_event_.SetURL(GURL("http://www.something-else.com"));
    empty_url_event_.SetURL(GURL());
  }

 protected:
  scoped_ptr<base::Value> HostSuffixDict(const std::string& host_suffix) {
    scoped_ptr<base::DictionaryValue> dict(new DictionaryValue());
    dict->Set("hostSuffix", new base::StringValue(host_suffix));
    return scoped_ptr<base::Value>(dict.release());
  }

  scoped_ptr<base::ListValue> ValueAsList(scoped_ptr<base::Value> value) {
    scoped_ptr<base::ListValue> result(new base::ListValue());
    result->Append(value.release());
    return result.Pass();
  }

  scoped_ptr<EventMatcher> AllURLs() {
    return scoped_ptr<EventMatcher>(new EventMatcher(
        scoped_ptr<DictionaryValue>(new DictionaryValue), MSG_ROUTING_NONE));
  }

  scoped_ptr<EventMatcher> HostSuffixMatcher(const std::string& host_suffix) {
    return MatcherFromURLFilterList(ValueAsList(HostSuffixDict(host_suffix)));
  }

  scoped_ptr<EventMatcher> MatcherFromURLFilterList(
      scoped_ptr<ListValue> url_filter_list) {
    scoped_ptr<DictionaryValue> filter_dict(new DictionaryValue);
    filter_dict->Set("url", url_filter_list.release());
    return scoped_ptr<EventMatcher>(
        new EventMatcher(filter_dict.Pass(), MSG_ROUTING_NONE));
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
  info.SetURL(GURL("http://www.yahoo.com"));
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
  info.SetURL(GURL("http://www.google.com"));
  int id = event_filter_.AddEventMatcher("event1",
                                         HostSuffixMatcher("google.com"));
  std::set<int> matches = event_filter_.MatchEvent(
      "event1", info, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest, TestMultipleURLFiltersMatchOnAny) {
  scoped_ptr<base::ListValue> filters(new base::ListValue());
  filters->Append(HostSuffixDict("google.com").release());
  filters->Append(HostSuffixDict("yahoo.com").release());

  scoped_ptr<EventMatcher> matcher(MatcherFromURLFilterList(filters.Pass()));
  int id = event_filter_.AddEventMatcher("event1", matcher.Pass());

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
  ASSERT_EQ(0, event_filter_.GetMatcherCountForEvent("event1"));
  int id1 = event_filter_.AddEventMatcher("event1", AllURLs());
  ASSERT_EQ(1, event_filter_.GetMatcherCountForEvent("event1"));
  int id2 = event_filter_.AddEventMatcher("event1", AllURLs());
  ASSERT_EQ(2, event_filter_.GetMatcherCountForEvent("event1"));
  event_filter_.RemoveEventMatcher(id1);
  ASSERT_EQ(1, event_filter_.GetMatcherCountForEvent("event1"));
  event_filter_.RemoveEventMatcher(id2);
  ASSERT_EQ(0, event_filter_.GetMatcherCountForEvent("event1"));
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
  scoped_ptr<base::ListValue> filter_list(new base::ListValue());
  filter_list->Append(new base::ListValue());  // Should be a dict.
  scoped_ptr<EventMatcher> matcher(MatcherFromURLFilterList(
      filter_list.Pass()));
  int id1 = event_filter_.AddEventMatcher("event1", matcher.Pass());
  EXPECT_TRUE(event_filter_.IsURLMatcherEmpty());
  ASSERT_EQ(-1, id1);
}

TEST_F(EventFilterUnittest, EmptyListOfURLFiltersMatchesAllURLs) {
  scoped_ptr<base::ListValue> filter_list(new base::ListValue());
  scoped_ptr<EventMatcher> matcher(MatcherFromURLFilterList(
      scoped_ptr<ListValue>(new ListValue)));
  int id = event_filter_.AddEventMatcher("event1", matcher.Pass());
  std::set<int> matches = event_filter_.MatchEvent("event1",
      google_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

TEST_F(EventFilterUnittest,
    InternalURLMatcherShouldBeEmptyWhenThereAreNoEventMatchers) {
  ASSERT_TRUE(event_filter_.IsURLMatcherEmpty());
  int id = event_filter_.AddEventMatcher("event1",
                                         HostSuffixMatcher("google.com"));
  ASSERT_FALSE(event_filter_.IsURLMatcherEmpty());
  event_filter_.RemoveEventMatcher(id);
  ASSERT_TRUE(event_filter_.IsURLMatcherEmpty());
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
  scoped_ptr<EventMatcher> matcher(MatcherFromURLFilterList(ValueAsList(
      scoped_ptr<Value>(new DictionaryValue()))));
  int id = event_filter_.AddEventMatcher("event1", matcher.Pass());
  std::set<int> matches = event_filter_.MatchEvent(
      "event1", empty_url_event_, MSG_ROUTING_NONE);
  ASSERT_EQ(1u, matches.size());
  ASSERT_EQ(1u, matches.count(id));
}

}  // namespace extensions
