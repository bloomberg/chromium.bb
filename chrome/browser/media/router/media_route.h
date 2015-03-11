// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/media/router/media_route_id.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "url/gurl.h"

namespace media_router {

using RouteRequestId = int64;

class MediaRouteFactory;

// For now, a simple transition graph: NEW -> CONNECTED -> CLOSED.
enum MediaRouteState {
  // The route is new and not yet connected to a sink.
  MEDIA_ROUTE_STATE_NEW,
  // The route is active and connected to a sink.
  MEDIA_ROUTE_STATE_CONNECTED,
  // The route has been disconnected.
  MEDIA_ROUTE_STATE_CLOSED
};

// MediaRoute objects contain the status and metadata of a routing
// operation. The fields are immutable and reflect the route status
// only at the time of object creation. Updated route statuses must
// be retrieved as new MediaRoute objects from the Media Router.
class MediaRoute {
 public:
  // |media_route_id|: ID of the route. New route IDs should be created
  //                   by the RouteIdManager class.
  // |media_source|: Description of source of the route.
  // |media_sink|: The sink that is receiving the media.
  // |description|: Description of the route to be displayed.
  // |is_local|: true if the route was created from this browser.
  MediaRoute(const MediaRouteId& media_route_id,
             const MediaSource& media_source,
             const MediaSink& media_sink,
             const std::string& description,
             bool is_local);

  ~MediaRoute();

  // The media route identifier.
  const MediaRouteId& media_route_id() const { return media_route_id_; }

  // The state of the media route.
  MediaRouteState state() const { return state_; }

  // The media source being routed.
  const MediaSource& media_source() const { return media_source_; }

  // The sink being routed to.
  const MediaSink& media_sink() const { return media_sink_; }

  // The description of the media route activity, for example
  // "Playing Foo Bar Music All Access."
  // TODO(kmarshall): Do we need to pass locale for bidi rendering?
  const std::string& description() const { return description_; }

  // Returns |true| if the route is created locally (versus discovered
  // by a media route provider.)
  bool is_local() const { return is_local_; }

  bool Equals(const MediaRoute& other) const;

 private:
  const MediaRouteId media_route_id_;
  const MediaSource media_source_;
  const MediaSink media_sink_;
  const std::string description_;
  const bool is_local_;
  const MediaRouteState state_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_H_
