// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"

namespace media_router {

class IssuesObserver;
class MediaRoutesObserver;
class MediaSinksObserver;

// Type of callback used in |CreateRoute()|. Callback is invoked when the
// route request either succeeded or failed.
// The first argument is the route created. If the route request failed, this
// will be a nullptr.
// The second argument is the error string, which will be nonempty if the route
// request failed.
using MediaRouteResponseCallback =
    base::Callback<void(scoped_ptr<MediaRoute>, const std::string&)>;

// Used in cases where a tab ID is not applicable in CreateRoute/JoinRoute.
const int kInvalidTabId = -1;

// An interface for handling resources related to media routing.
// Responsible for registering observers for receiving sink availability
// updates, handling route requests/responses, and operating on routes (e.g.
// posting messages or closing).
class MediaRouter {
 public:
  using SendRouteMessageCallback = base::Callback<void(bool sent)>;

  virtual ~MediaRouter() {}

  // Creates a media route from |source_id| to |sink_id|.
  // |origin| is the URL of requestor's page.
  // |tab_id| is the ID of the tab in which the request was made.
  // |origin| and |tab_id| are used for enforcing same-origin and/or same-tab
  // scope for JoinRoute() requests. (e.g., if enforced, the page
  // requesting JoinRoute() must have the same origin as the page that requested
  // CreateRoute()).
  // The caller may pass in|kInvalidTabId| if tab is not applicable.
  // |callback| is invoked with a response indicating success or failure.
  virtual void CreateRoute(const MediaSource::Id& source_id,
                           const MediaSink::Id& sink_id,
                           const GURL& origin,
                           int tab_id,
                           const MediaRouteResponseCallback& callback) = 0;

  // Joins an existing route identified by |presentation_id|.
  // |source|: The source to route to the existing route.
  // |presentation_id|: Presentation ID of the existing route.
  // |origin|, |tab_id|: Origin and tab of the join route request. Used for
  // validation when enforcing same-origin and/or same-tab scope.
  // (See CreateRoute documentation).
  // |callback| is invoked with a response indicating success or failure.
  virtual void JoinRoute(const MediaSource::Id& source,
                         const std::string& presentation_id,
                         const GURL& origin,
                         int tab_id,
                         const MediaRouteResponseCallback& callback) = 0;

  // Closes the media route specified by |route_id|.
  virtual void CloseRoute(const MediaRoute::Id& route_id) = 0;

  // Posts |message| to a MediaSink connected via MediaRoute with |route_id|.
  // TODO(imcheng): Support additional data types: Blob, ArrayBuffer,
  // ArrayBufferView.
  virtual void SendRouteMessage(const MediaRoute::Id& route_id,
                                const std::string& message,
                                const SendRouteMessageCallback& callback) = 0;

  // Clears the issue with the id |issue_id|.
  virtual void ClearIssue(const Issue::Id& issue_id) = 0;

  // Receives updates from a MediaRouter instance.
  class Delegate {
   public:
    // Called when there is a message from a route.
    // |route_id|: The route ID.
    // |message|: The message.
    virtual void OnMessage(const MediaRoute::Id& route_id,
                           const std::string& message) = 0;
  };

 private:
  friend class IssuesObserver;
  friend class MediaSinksObserver;
  friend class MediaRoutesObserver;

  // The following functions are called by IssuesObserver, MediaSinksObserver,
  // and MediaRoutesObserver.

  // Registers |observer| with this MediaRouter. |observer| specifies a media
  // source and will receive updates with media sinks that are compatible with
  // that source. The initial update may happen synchronously.
  // NOTE: This class does not assume ownership of |observer|. Callers must
  // manage |observer| and make sure |UnregisterObserver()| is called
  // before the observer is destroyed.
  // It is invalid to register the same observer more than once and will result
  // in undefined behavior.
  // If the MRPM Host is not available, the registration request will fail
  // immediately.
  virtual void RegisterMediaSinksObserver(MediaSinksObserver* observer) = 0;

  // Removes a previously added MediaSinksObserver. |observer| will stop
  // receiving further updates.
  virtual void UnregisterMediaSinksObserver(MediaSinksObserver* observer) = 0;

  // Adds a MediaRoutesObserver to listen for updates on MediaRoutes.
  // The initial update may happen synchronously.
  // MediaRouter does not own |observer|. |RemoveMediaRoutesObserver| should
  // be called before |observer| is destroyed.
  // It is invalid to register the same observer more than once and will result
  // in undefined behavior.
  virtual void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) = 0;

  // Removes a previously added MediaRoutesObserver. |observer| will stop
  // receiving further updates.
  virtual void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) = 0;

  // Adds the IssuesObserver |observer|.
  // It is invalid to register the same observer more than once and will result
  // in undefined behavior.
  virtual void AddIssuesObserver(IssuesObserver* observer) = 0;

  // Removes the IssuesObserver |observer|.
  virtual void RemoveIssuesObserver(IssuesObserver* observer) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_H_
