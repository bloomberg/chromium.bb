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
#include "chrome/browser/media/router/media_router.mojom.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "extensions/browser/event_page_tracker.h"
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

  if (arg.help_url() != other.help_url())
    return false;

  return true;
}

class MockIssuesObserver : public IssuesObserver {
 public:
  explicit MockIssuesObserver(MediaRouter* router);
  ~MockIssuesObserver() override;

  MOCK_METHOD1(OnIssueUpdated, void(const Issue* issue));
};

class MockMediaRouteProvider : public interfaces::MediaRouteProvider {
 public:
  MockMediaRouteProvider();
  ~MockMediaRouteProvider() override;

  MOCK_METHOD7(CreateRoute,
               void(const mojo::String& source_urn,
                    const mojo::String& sink_id,
                    const mojo::String& presentation_id,
                    const mojo::String& origin,
                    int tab_id,
                    int64_t timeout_secs,
                    const CreateRouteCallback& callback));
  MOCK_METHOD6(JoinRoute,
               void(const mojo::String& source_urn,
                    const mojo::String& presentation_id,
                    const mojo::String& origin,
                    int tab_id,
                    int64_t timeout_secs,
                    const JoinRouteCallback& callback));
  MOCK_METHOD7(ConnectRouteByRouteId,
               void(const mojo::String& source_urn,
                    const mojo::String& route_id,
                    const mojo::String& presentation_id,
                    const mojo::String& origin,
                    int tab_id,
                    int64_t timeout_secs,
                    const JoinRouteCallback& callback));
  MOCK_METHOD1(DetachRoute, void(const mojo::String& route_id));
  MOCK_METHOD1(TerminateRoute, void(const mojo::String& route_id));
  MOCK_METHOD1(StartObservingMediaSinks, void(const mojo::String& source));
  MOCK_METHOD1(StopObservingMediaSinks, void(const mojo::String& source));
  MOCK_METHOD3(SendRouteMessage,
               void(const mojo::String& media_route_id,
                    const mojo::String& message,
                    const SendRouteMessageCallback& callback));
  void SendRouteBinaryMessage(
      const mojo::String& media_route_id,
      mojo::Array<uint8_t> data,
      const SendRouteMessageCallback& callback) override {
    SendRouteBinaryMessageInternal(media_route_id, data.storage(), callback);
  }
  MOCK_METHOD3(SendRouteBinaryMessageInternal,
               void(const mojo::String& media_route_id,
                    const std::vector<uint8_t>& data,
                    const SendRouteMessageCallback& callback));
  MOCK_METHOD2(ListenForRouteMessages,
               void(const mojo::String& route_id,
                    const ListenForRouteMessagesCallback& callback));
  MOCK_METHOD1(StopListeningForRouteMessages,
               void(const mojo::String& route_id));
  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const mojo::String& route_id));
  MOCK_METHOD1(StartObservingMediaRoutes, void(const mojo::String& source));
  MOCK_METHOD1(StopObservingMediaRoutes, void(const mojo::String& source));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaRouteProvider);
};

class MockMediaSinksObserver : public MediaSinksObserver {
 public:
  MockMediaSinksObserver(MediaRouter* router, const MediaSource& source);
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

class MockEventPageTracker : public extensions::EventPageTracker {
 public:
  MockEventPageTracker();
  ~MockEventPageTracker();

  MOCK_METHOD1(IsEventPageSuspended, bool(const std::string& extension_id));
  MOCK_METHOD2(WakeEventPage,
               bool(const std::string& extension_id,
                    const base::Callback<void(bool)>& callback));
};

class MockPresentationConnectionStateChangedCallback {
 public:
  MockPresentationConnectionStateChangedCallback();
  ~MockPresentationConnectionStateChangedCallback();
  MOCK_METHOD1(Run, void(content::PresentationConnectionState));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_HELPER_H_
