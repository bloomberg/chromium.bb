// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_HELPER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

// Matcher for objects that uses Equals() member function for equality check.
MATCHER_P(Equals, other, "") {
  return arg.Equals(other);
}

// Matcher for a sequence of objects that uses Equals() member function for
// equality check.
MATCHER_P(SequenceEquals, other, "") {
  if (arg.size() != other.size()) {
    return false;
  }
  for (size_t i = 0; i < arg.size(); ++i) {
    if (!arg[i].Equals(other[i])) {
      return false;
    }
  }
  return true;
}

// Matcher for checking all fields in Issue objects except the ID.
MATCHER_P(EqualsIssue, other, "") {
  if (arg.title() != other.title())
    return false;

  if (arg.message() != other.message())
    return false;

  if (!arg.default_action().Equals(other.default_action()))
    return false;

  if (arg.secondary_actions().size() != other.secondary_actions().size())
    return false;

  for (size_t i = 0; i < arg.secondary_actions().size(); ++i) {
    if (!arg.secondary_actions()[i].Equals(other.secondary_actions()[i]))
      return false;
  }

  if (arg.route_id() != other.route_id())
    return false;

  if (arg.severity() != other.severity())
    return false;

  if (arg.is_blocking() != other.is_blocking())
    return false;

  if (arg.help_page_id() != other.help_page_id())
    return false;

  return true;
}

MATCHER_P(IssueTitleEquals, title, "") {
  return arg.title() == title;
}

MATCHER_P(StateChageInfoEquals, other, "") {
  return arg.state == other.state && arg.close_reason == other.close_reason &&
         arg.message == other.message;
}

class MockIssuesObserver : public IssuesObserver {
 public:
  explicit MockIssuesObserver(MediaRouter* router);
  ~MockIssuesObserver() override;

  MOCK_METHOD1(OnIssueUpdated, void(const Issue* issue));
};

class MockMediaSinksObserver : public MediaSinksObserver {
 public:
  MockMediaSinksObserver(MediaRouter* router,
                         const MediaSource& source,
                         const GURL& origin);
  ~MockMediaSinksObserver() override;

  MOCK_METHOD1(OnSinksReceived, void(const std::vector<MediaSink>& sinks));
};

class MockMediaRoutesObserver : public MediaRoutesObserver {
 public:
  explicit MockMediaRoutesObserver(MediaRouter* router,
      const MediaSource::Id source_id = std::string());
  ~MockMediaRoutesObserver() override;

  MOCK_METHOD2(OnRoutesUpdated, void(const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids));
};

class MockPresentationConnectionStateChangedCallback {
 public:
  MockPresentationConnectionStateChangedCallback();
  ~MockPresentationConnectionStateChangedCallback();
  MOCK_METHOD1(Run,
               void(const content::PresentationConnectionStateChangeInfo&));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_HELPER_H_
