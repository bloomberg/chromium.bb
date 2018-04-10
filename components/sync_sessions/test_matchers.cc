// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/test_matchers.h"

#include "components/sessions/core/session_id.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {
namespace {

using testing::ElementsAreArray;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;

class MatchesHeaderMatcher
    : public MatcherInterface<const sync_pb::SessionSpecifics&> {
 public:
  MatchesHeaderMatcher(Matcher<std::string> session_tag,
                       Matcher<std::vector<int>> window_ids,
                       Matcher<std::vector<int>> tab_ids)
      : session_tag_(session_tag), window_ids_(window_ids), tab_ids_(tab_ids) {}

  bool MatchAndExplain(const sync_pb::SessionSpecifics& actual,
                       MatchResultListener* listener) const override {
    if (!actual.has_header()) {
      *listener << "which is not a header entity";
      return false;
    }
    if (!session_tag_.MatchAndExplain(actual.session_tag(), listener)) {
      *listener << "which contains an unexpected session tag: "
                << actual.session_tag();
      return false;
    }
    std::vector<int> actual_window_ids;
    std::vector<int> actual_tab_ids;
    for (const auto& window : actual.header().window()) {
      actual_window_ids.push_back(window.window_id());
      actual_tab_ids.insert(actual_tab_ids.end(), window.tab().begin(),
                            window.tab().end());
    }
    if (!window_ids_.MatchAndExplain(actual_window_ids, listener)) {
      *listener << "which contains an unexpected windows";
      return false;
    }
    if (!tab_ids_.MatchAndExplain(actual_tab_ids, listener)) {
      *listener << "which contains an unexpected tabs";
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "matches expected header";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not match expected header";
  }

 private:
  Matcher<std::string> session_tag_;
  Matcher<std::vector<int>> window_ids_;
  Matcher<std::vector<int>> tab_ids_;
};

class MatchesTabMatcher
    : public MatcherInterface<const sync_pb::SessionSpecifics&> {
 public:
  MatchesTabMatcher(Matcher<std::string> session_tag,
                    Matcher<int> window_id,
                    Matcher<int> tab_id,
                    Matcher<std::vector<std::string>> urls)
      : session_tag_(session_tag),
        window_id_(window_id),
        tab_id_(tab_id),
        urls_(urls) {}

  bool MatchAndExplain(const sync_pb::SessionSpecifics& actual,
                       MatchResultListener* listener) const override {
    if (!actual.has_tab()) {
      *listener << "which is not a tab entity";
      return false;
    }
    if (!session_tag_.MatchAndExplain(actual.session_tag(), listener)) {
      *listener << "which contains an unexpected session tag: "
                << actual.session_tag();
      return false;
    }
    if (!window_id_.MatchAndExplain(actual.tab().window_id(), listener)) {
      *listener << "which contains an unexpected window ID: "
                << actual.tab().window_id();
      return false;
    }
    if (!tab_id_.MatchAndExplain(actual.tab().tab_id(), listener)) {
      *listener << "which contains an unexpected tab ID: "
                << actual.tab().tab_id();
      return false;
    }
    std::vector<std::string> actual_urls;
    for (const sync_pb::TabNavigation& navigation : actual.tab().navigation()) {
      actual_urls.push_back(navigation.virtual_url());
    }
    if (!urls_.MatchAndExplain(actual_urls, listener)) {
      *listener << "which contains unexpected navigation URLs";
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "matches expected tab";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not match expected tab";
  }

 private:
  Matcher<std::string> session_tag_;
  Matcher<int> window_id_;
  Matcher<int> tab_id_;
  Matcher<std::vector<std::string>> urls_;
};

class MatchesSyncedSessionMatcher
    : public MatcherInterface<const SyncedSession*> {
 public:
  MatchesSyncedSessionMatcher(Matcher<std::string> session_tag,
                              Matcher<std::vector<int>> window_ids,
                              Matcher<std::vector<int>> tab_ids)
      : session_tag_(session_tag), window_ids_(window_ids), tab_ids_(tab_ids) {}

  bool MatchAndExplain(const SyncedSession* actual,
                       MatchResultListener* listener) const override {
    if (!actual) {
      *listener << "which is null";
      return false;
    }
    if (!session_tag_.MatchAndExplain(actual->session_tag, listener)) {
      *listener << "which contains an unexpected session tag: "
                << actual->session_tag;
      return false;
    }
    std::vector<int> actual_window_ids;
    std::vector<int> actual_tab_ids;
    for (const auto& id_and_window : actual->windows) {
      actual_window_ids.push_back(
          id_and_window.second->wrapped_window.window_id.id());
      for (const auto& id : id_and_window.second->wrapped_window.tabs) {
        actual_tab_ids.push_back(id->tab_id.id());
      }
    }
    if (!window_ids_.MatchAndExplain(actual_window_ids, listener)) {
      *listener << "which contains an unexpected window set";
      return false;
    }
    if (!tab_ids_.MatchAndExplain(actual_tab_ids, listener)) {
      *listener << "which contains an unexpected tab set";
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "matches expected synced session";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not match expected synced session";
  }

 private:
  Matcher<std::string> session_tag_;
  Matcher<std::vector<int>> window_ids_;
  Matcher<std::vector<int>> tab_ids_;
};

}  // namespace

Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    Matcher<std::string> session_tag,
    Matcher<std::vector<int>> window_ids,
    Matcher<std::vector<int>> tab_ids) {
  return testing::MakeMatcher(
      new MatchesHeaderMatcher(session_tag, window_ids, tab_ids));
}

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    const std::vector<int>& window_ids,
    const std::vector<int>& tab_ids) {
  return MatchesHeader(session_tag, ElementsAreArray(window_ids),
                       ElementsAreArray(tab_ids));
}

Matcher<const sync_pb::SessionSpecifics&> MatchesTab(
    Matcher<std::string> session_tag,
    Matcher<int> window_id,
    Matcher<int> tab_id,
    Matcher<std::vector<std::string>> urls) {
  return testing::MakeMatcher(
      new MatchesTabMatcher(session_tag, window_id, tab_id, urls));
}

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesTab(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<int> window_id,
    testing::Matcher<int> tab_id,
    const std::vector<std::string>& urls) {
  return MatchesTab(session_tag, window_id, tab_id, ElementsAreArray(urls));
}

Matcher<const SyncedSession*> MatchesSyncedSession(
    Matcher<std::string> session_tag,
    Matcher<std::vector<int>> window_ids,
    Matcher<std::vector<int>> tab_ids) {
  return testing::MakeMatcher(
      new MatchesSyncedSessionMatcher(session_tag, window_ids, tab_ids));
}

testing::Matcher<const SyncedSession*> MatchesSyncedSession(
    testing::Matcher<std::string> session_tag,
    const std::vector<int>& window_ids,
    const std::vector<int>& tab_ids) {
  return MatchesSyncedSession(session_tag, ElementsAreArray(window_ids),
                              ElementsAreArray(tab_ids));
}

}  // namespace sync_sessions
