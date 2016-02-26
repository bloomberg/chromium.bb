// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_H_

#include <string>

#include "base/containers/small_map.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "content/public/browser/presentation_session.h"
#include "url/gurl.h"

namespace media_router {

// MediaRoute objects contain the status and metadata of a routing
// operation. The fields are immutable and reflect the route status
// only at the time of object creation. Updated route statuses must
// be retrieved as new MediaRoute objects from the Media Router.
//
// TODO(mfoltz): Convert to a simple struct and remove uncommon parameters from
// the ctor.
class MediaRoute {
 public:
  using Id = std::string;

  // |media_route_id|: ID of the route.
  // |media_source|: Description of source of the route.
  // |media_sink|: The sink that is receiving the media.
  // |description|: Description of the route to be displayed.
  // |is_local|: true if the route was created from this browser.
  // |custom_controller_path|: custom controller path if it is given by route
  //     provider. empty otherwise.
  // |for_display|: Set to true if this route should be displayed for
  //     |media_sink_id| in UI.
  MediaRoute(const MediaRoute::Id& media_route_id,
             const MediaSource& media_source,
             const MediaSink::Id& media_sink_id,
             const std::string& description,
             bool is_local,
             const std::string& custom_controller_path,
             bool for_display);
  MediaRoute(const MediaRoute& other);
  ~MediaRoute();

  // The media route identifier.
  const MediaRoute::Id& media_route_id() const { return media_route_id_; }

  // The media source being routed.
  const MediaSource& media_source() const { return media_source_; }

  // The ID of sink being routed to.
  const MediaSink::Id& media_sink_id() const { return media_sink_id_; }

  // The description of the media route activity, for example
  // "Playing Foo Bar Music All Access."
  // TODO(kmarshall): Do we need to pass locale for bidi rendering?
  const std::string& description() const { return description_; }

  // Returns |true| if the route is created locally (versus discovered
  // by a media route provider.)
  bool is_local() const { return is_local_; }

  // The custom controller path. This allows route provider to have custom route
  // detail as well as its own route control features route control features in
  // the media router dialog.
  const std::string& custom_controller_path() const {
    return custom_controller_path_;
  }

  bool for_display() const { return for_display_; }

  // Sets off_the_record.  Set this to true when the route was created by an off
  // the record (incognito) profile.
  void set_off_the_record(bool off_the_record) {
    off_the_record_ = off_the_record;
  }

  bool off_the_record() const { return off_the_record_; }

  bool Equals(const MediaRoute& other) const;

 private:
  MediaRoute::Id media_route_id_;
  MediaSource media_source_;
  MediaSink::Id media_sink_id_;
  std::string description_;
  bool is_local_;
  std::string custom_controller_path_;
  bool for_display_;
  bool off_the_record_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_H_
